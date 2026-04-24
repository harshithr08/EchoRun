import { spawn } from "node:child_process";
import fs from "node:fs/promises";
import path from "node:path";
import process from "node:process";

import {
  cleanupOutputDir,
  enrichEvents,
  findDivergence,
  formatCommand,
  parseTraceBin,
  readJsonLines,
  readTextIfExists,
  relPath,
  renderTerminalStory,
  summarizeLanes,
  writeJson,
} from "./artifact-lib-CHANGE.mjs";

const rootDir = path.resolve(import.meta.dirname, "..");
const recorderDir = path.join(rootDir, "final", "recorder");
const replayerDir = path.join(rootDir, "final", "replayer");
const outDir = path.join(rootDir, "out");
const tmpDir = path.join(outDir, "tmp-CHANGE");

const recorderBin = path.join(recorderDir, "echorun");
const replayerBin = path.join(replayerDir, "echoplay");
const targets = [
  {
    slug: "stdin-capture",
    label: "stdin capture",
    description: "Record and replay a typed stdin read.",
    command: ["./tracee"],
    recordInput: "hello\n",
    replayCommand: ["./tracee"],
    commandCwd: recorderDir,
    replayCwd: recorderDir,
  },
  {
    slug: "getrandom-capture",
    label: "getrandom capture",
    description: "Capture getrandom, hostname reads, and stdin replay.",
    command: ["./tracee2"],
    recordInput: "harshith\n",
    replayCommand: ["./tracee2"],
    commandCwd: recorderDir,
    replayCwd: recorderDir,
  },
  {
    slug: "file-io",
    label: "file io mirror",
    description: "Mirror file reads across record and replay.",
    command: ["./tracee3"],
    replayCommand: ["./tracee3"],
    commandCwd: recorderDir,
    replayCwd: recorderDir,
  },
  {
    slug: "real-binary-ls",
    label: "real binary /bin/ls",
    description: "Record and stress-replay a real system binary.",
    command: ["/bin/ls", "-1", path.join(rootDir, "final")],
    replayCommand: ["/bin/ls", "-1", path.join(rootDir, "final")],
    commandCwd: rootDir,
    replayCwd: rootDir,
    replayOkExitCodes: [0, 1],
  },
  {
    slug: "divergence-detection",
    label: "divergence detection",
    description: "Replay a mismatched binary and capture the divergence point.",
    command: ["./tracee"],
    recordInput: "world\n",
    replayCommand: ["/bin/ls", "-1", path.join(rootDir, "final")],
    replayOkExitCodes: [0, 1],
    commandCwd: recorderDir,
    replayCwd: rootDir,
  },
  {
    slug: "time-travel-repl",
    label: "time-travel repl",
    description: "Drive step and goto through a scripted REPL session.",
    command: ["./tracee"],
    recordInput: "debug\n",
    replayCommand: ["./tracee"],
    generateReplRun: true,
    commandCwd: recorderDir,
    replayCwd: recorderDir,
  },
];

await cleanupOutputDir(outDir);
await fs.mkdir(tmpDir, { recursive: true });

const manifestTargets = [];

for (const target of targets) {
  const workDir = path.join(tmpDir, target.slug);
  await fs.mkdir(workDir, { recursive: true });

  console.log(`\n=== ${target.label} ===`);

  const recordEventsPath = path.join(workDir, "record.events.jsonl");
  const replayEventsPath = path.join(workDir, "replay.events.jsonl");
  const replEventsPath = path.join(workDir, "repl.events.jsonl");
  const replScriptPath = path.join(workDir, "repl-script.txt");
  const traceBinPath = path.join(workDir, "trace.bin");
  const traceIdxPath = path.join(workDir, "trace.idx");

  const recordLogPath = path.join(outDir, `${target.slug}.record.txt`);
  const replayLogPath = path.join(outDir, `${target.slug}.replay.txt`);
  const replLogPath = path.join(outDir, `${target.slug}.repl.txt`);

  await runAndCapture({
    command: recorderBin,
    args: ["--events-jsonl", recordEventsPath, ...target.command],
    cwd: target.commandCwd ?? recorderDir,
    input: target.recordInput ?? "",
    transcriptPath: recordLogPath,
  });

  await fs.copyFile(path.join(recorderDir, "trace.bin"), traceBinPath);
  await fs.copyFile(path.join(recorderDir, "trace.idx"), traceIdxPath);

  await runAndCapture({
    command: replayerBin,
    args: ["--events-jsonl", replayEventsPath, traceBinPath, traceIdxPath, ...target.replayCommand],
    cwd: target.replayCwd ?? replayerDir,
    transcriptPath: replayLogPath,
    okExitCodes: target.replayOkExitCodes ?? [0],
  });

  const trace = await parseTraceBin(traceBinPath);

  if (target.generateReplRun) {
    const replScript = buildReplScript(trace.events);
    await fs.writeFile(replScriptPath, replScript, "utf8");

    await runAndCapture({
      command: replayerBin,
      args: [
        "--events-jsonl",
        replEventsPath,
        "--step",
        "--repl-script",
        replScriptPath,
        traceBinPath,
        traceIdxPath,
        ...target.replayCommand,
      ],
      cwd: target.replayCwd ?? replayerDir,
      transcriptPath: replLogPath,
    });
  }

  const recordEvents = enrichEvents(await readJsonLines(recordEventsPath), trace.bySeq);
  const replayEvents = enrichEvents(await readJsonLines(replayEventsPath), trace.bySeq);
  const replEvents = enrichEvents(await readJsonLines(replEventsPath), trace.bySeq);

  const divergence = findDivergence(replayEvents, replEvents);
  const referenceEvents = recordEvents.length > 0 ? recordEvents : replayEvents;

  console.log(renderTerminalStory(target.label, referenceEvents, divergence));

  const artifact = await buildArtifact({
    target,
    trace,
    recordEvents,
    replayEvents,
    replEvents,
    recordLogPath,
    replayLogPath,
    replLogPath: target.generateReplRun ? replLogPath : null,
    replScriptPath: target.generateReplRun ? replScriptPath : null,
    divergence,
  });

  const artifactPath = path.join(outDir, `${target.slug}.artifact.json`);
  await writeJson(artifactPath, artifact);

  manifestTargets.push({
    slug: target.slug,
    label: target.label,
    description: target.description,
    command: artifact.command,
    eventCount: artifact.stats.totalEvents,
    diverged: Boolean(divergence),
    artifactPath: `/out/${target.slug}.artifact.json`,
  });
}

await writeJson(path.join(outDir, "manifest.json"), {
  generatedAt: new Date().toISOString(),
  targets: manifestTargets,
});

async function buildArtifact({
  target,
  trace,
  recordEvents,
  replayEvents,
  replEvents,
  recordLogPath,
  replayLogPath,
  replLogPath,
  replScriptPath,
  divergence,
}) {
  const recordLog = await readTextIfExists(recordLogPath);
  const replayLog = await readTextIfExists(replayLogPath);
  const replLog = replLogPath ? await readTextIfExists(replLogPath) : null;
  const replScript = replScriptPath ? await readTextIfExists(replScriptPath) : null;

  const referenceEvents = recordEvents.length > 0 ? recordEvents : replayEvents;
  const totalEvents = Math.max(recordEvents.length, replayEvents.length, replEvents.length);
  const maxSeq = referenceEvents.length > 0 ? Math.max(...referenceEvents.map((event) => event.seq)) : 0;

  return {
    slug: target.slug,
    label: target.label,
    description: target.description,
    generatedAt: new Date().toISOString(),
    command: formatCommand(target.command.map(displayCommandPart)),
    replayCommand: formatCommand(target.replayCommand.map(displayCommandPart)),
    replCommand: replScript ? "scripted step/goto replay" : null,
    divergence: divergence
      ? {
          seq: divergence.seq,
          expectedSyscallNo: divergence.expectedSyscallNo,
          expectedSyscallName: divergence.expectedSyscallName,
          actualSyscallNo: divergence.actualSyscallNo,
          actualSyscallName: divergence.actualSyscallName,
          message: `expected ${divergence.expectedSyscallName}(), got ${divergence.actualSyscallName}()`,
        }
      : null,
    stats: {
      totalEvents,
      maxSeq,
      traceEventCount: trace.header.eventCount,
      recordLaneCounts: summarizeLanes(recordEvents),
      replayLaneCounts: summarizeLanes(replayEvents),
      replLaneCounts: summarizeLanes(replEvents),
    },
    trace: {
      eventCount: trace.header.eventCount,
      events: trace.events,
    },
    record: {
      events: recordEvents,
      logPath: `/out/${target.slug}.record.txt`,
      log: recordLog,
    },
    replay: {
      events: replayEvents,
      logPath: `/out/${target.slug}.replay.txt`,
      log: replayLog,
    },
    repl: replLogPath && replLog
      ? {
          events: replEvents,
          logPath: `/out/${target.slug}.repl.txt`,
          log: replLog,
          script: replScript,
        }
      : null,
  };
}

function buildReplScript(traceEvents) {
  const candidate = traceEvents.find((event) => event.eventType === 1) ?? traceEvents[0];
  const gotoSeq = candidate ? candidate.seqIdx : 0;
  return `checkpoints\nstep\ngoto ${gotoSeq}\nquit\n`;
}

async function runAndCapture({ command, args, cwd, input = "", transcriptPath, okExitCodes = [0] }) {
  await fs.mkdir(path.dirname(transcriptPath), { recursive: true });

  return new Promise((resolve, reject) => {
    const child = spawn(command, args, {
      cwd,
      stdio: ["pipe", "pipe", "pipe"],
    });

    const chunks = [];

    const append = (data) => {
      const text = data.toString();
      chunks.push(text);
    };

    child.stdout.on("data", append);
    child.stderr.on("data", append);
    child.on("error", reject);
    child.stdin.on("error", (error) => {
      if (error.code !== "EPIPE") reject(error);
    });

    child.on("close", async (code) => {
      const output = chunks.join("");
      await fs.writeFile(transcriptPath, output, "utf8");

      if (!okExitCodes.includes(code ?? 0)) {
        reject(
          new Error(
            `Command failed (${code}): ${formatCommand([displayCommandPart(command), ...args.map(displayCommandPart)])}`,
          ),
        );
        return;
      }

      resolve({ code, output });
    });

    if (input.length > 0) child.stdin.write(input);
    child.stdin.end();
  });
}

function displayCommandPart(part) {
  if (!part.startsWith("/")) return part;
  if (part.startsWith(rootDir + path.sep)) return relPath(rootDir, part);
  return part;
}
