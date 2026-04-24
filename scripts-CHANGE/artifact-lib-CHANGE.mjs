import { createHash } from "node:crypto";
import fs from "node:fs/promises";
import path from "node:path";

const ANSI = {
  reset: "\x1b[0m",
  bold: "\x1b[1m",
  dim: "\x1b[2m",
  red: "\x1b[31m",
  green: "\x1b[32m",
  yellow: "\x1b[33m",
  blue: "\x1b[34m",
  magenta: "\x1b[35m",
  cyan: "\x1b[36m",
  gray: "\x1b[90m",
  white: "\x1b[37m",
};

export const laneOrder = [
  "signals",
  "process",
  "ipc",
  "deterministic",
  "non_deterministic",
  "side_effect",
];

const laneMeta = {
  signals: { label: "signals", color: ANSI.magenta },
  process: { label: "process", color: ANSI.blue },
  ipc: { label: "ipc", color: ANSI.white },
  deterministic: { label: "deterministic", color: ANSI.cyan },
  non_deterministic: { label: "non-deterministic", color: ANSI.green },
  side_effect: { label: "side-effect", color: ANSI.yellow },
};

export async function cleanupOutputDir(outDir) {
  await fs.mkdir(outDir, { recursive: true });
  const entries = await fs.readdir(outDir, { withFileTypes: true });

  for (const entry of entries) {
    const fullPath = path.join(outDir, entry.name);
    if (entry.isDirectory() && entry.name === "tmp-CHANGE") {
      await fs.rm(fullPath, { recursive: true, force: true });
      continue;
    }

    if (!entry.isFile()) continue;

    if (
      entry.name === "manifest.json" ||
      entry.name.endsWith(".artifact.json") ||
      entry.name.endsWith(".record.txt") ||
      entry.name.endsWith(".replay.txt") ||
      entry.name.endsWith(".repl.txt")
    ) {
      await fs.rm(fullPath, { force: true });
    }
  }
}

export async function readJsonLines(filePath) {
  try {
    const raw = await fs.readFile(filePath, "utf8");
    return raw
      .split("\n")
      .map((line) => line.trim())
      .filter(Boolean)
      .map((line) => JSON.parse(line));
  } catch (error) {
    if (error.code === "ENOENT") return [];
    throw error;
  }
}

export async function readTextIfExists(filePath) {
  try {
    return await fs.readFile(filePath, "utf8");
  } catch (error) {
    if (error.code === "ENOENT") return null;
    throw error;
  }
}

export async function writeJson(filePath, value) {
  await fs.writeFile(filePath, `${JSON.stringify(value, null, 2)}\n`, "utf8");
}

export function shellQuote(value) {
  if (/^[A-Za-z0-9_./:-]+$/.test(value)) return value;
  return `'${value.replace(/'/g, `'\\''`)}'`;
}

export function formatCommand(parts) {
  return parts.map(shellQuote).join(" ");
}

export function relPath(rootDir, targetPath) {
  const relative = path.relative(rootDir, targetPath);
  return relative === "" ? "." : relative;
}

export async function parseTraceBin(tracePath) {
  const buffer = await fs.readFile(tracePath);
  const header = {
    magic: buffer.readUInt32LE(0),
    version: buffer.readUInt32LE(4),
    arch: buffer.readUInt32LE(8),
    eventCount: Number(buffer.readBigUInt64LE(12)),
  };

  const events = [];
  const bySeq = new Map();
  let offset = 20;

  for (let index = 0; index < header.eventCount; index += 1) {
    const event = {
      eventType: buffer.readUInt8(offset),
      syscallNo: buffer.readUInt32LE(offset + 1),
      seqIdx: Number(buffer.readBigUInt64LE(offset + 5)),
      retval: Number(buffer.readBigInt64LE(offset + 13)),
      dataLength: buffer.readUInt32LE(offset + 21),
      instructionCount: Number(buffer.readBigUInt64LE(offset + 25)),
    };
    offset += 33;

    const payload = buffer.subarray(offset, offset + event.dataLength);
    offset += event.dataLength;

    const preview = printablePreview(payload);
    const payloadHash = createHash("sha1").update(payload).digest("hex").slice(0, 12);

    const enriched = {
      ...event,
      payloadPreview: preview,
      payloadHash,
    };

    events.push(enriched);
    bySeq.set(event.seqIdx, enriched);
  }

  return { header, events, bySeq };
}

export function enrichEvents(events, traceBySeq) {
  return events.map((event) => {
    const traceEvent =
      event.traceSeq === null || event.traceSeq === undefined
        ? null
        : traceBySeq.get(event.traceSeq) ?? null;

    return {
      ...event,
      payloadSize: Math.max(event.payloadSize ?? 0, traceEvent?.dataLength ?? 0),
      payloadPreview: traceEvent?.payloadPreview ?? null,
      payloadHash: traceEvent?.payloadHash ?? null,
      instructionCount: traceEvent?.instructionCount ?? null,
      traceEventType: traceEvent?.eventType ?? null,
    };
  });
}

export function summarizeLanes(events) {
  const counts = Object.fromEntries(laneOrder.map((lane) => [lane, 0]));
  for (const event of events) {
    if (!(event.lane in counts)) continue;
    counts[event.lane] += 1;
  }
  return counts;
}

export function findDivergence(replayEvents, replEvents = []) {
  return [...replayEvents, ...replEvents].find((event) => event.diverged) ?? null;
}

export function renderTerminalStory(title, events, divergence) {
  const totalEvents = events.length;
  const maxSeq = totalEvents === 0 ? 0 : Math.max(...events.map((event) => event.seq));
  const termWidth = process.stdout.columns || 100;
  const width = Math.max(24, Math.min(96, termWidth - 28));
  const buckets = Object.fromEntries(laneOrder.map((lane) => [lane, Array(width).fill(0)]));

  for (const event of events) {
    const column = maxSeq === 0 ? 0 : Math.min(width - 1, Math.floor((event.seq / maxSeq) * (width - 1)));
    if (buckets[event.lane]) buckets[event.lane][column] += 1;
  }

  const divergenceColumn =
    divergence && maxSeq > 0 ? Math.min(width - 1, Math.floor((divergence.seq / maxSeq) * (width - 1))) : -1;

  const lines = [];
  lines.push(
    `${ANSI.bold}${title}${ANSI.reset} ${ANSI.dim}(${totalEvents} events)${ANSI.reset}`,
  );
  lines.push(`seq ${ANSI.gray}0${ANSI.reset}${" ".repeat(Math.max(1, width - 6))}${ANSI.gray}${maxSeq}${ANSI.reset}`);

  for (const lane of laneOrder) {
    const meta = laneMeta[lane];
    const label = meta.label.padEnd(17, " ");
    let row = `${meta.color}${label}${ANSI.reset} `;

    for (let index = 0; index < width; index += 1) {
      if (index === divergenceColumn) {
        row += `${ANSI.red}│${ANSI.reset}`;
        continue;
      }

      const count = buckets[lane][index];
      row += `${meta.color}${densityChar(count)}${ANSI.reset}`;
    }

    lines.push(row);
  }

  if (divergence) {
    const message = `divergence at seq ${divergence.seq}: expected ${divergence.expectedSyscallName}(), got ${divergence.actualSyscallName}()`;
    lines.push(`${ANSI.red}${message}${ANSI.reset}`);
  }

  lines.push(
    `${ANSI.dim}legend:${ANSI.reset} ${ANSI.magenta}sig${ANSI.reset} ${ANSI.blue}proc${ANSI.reset} ${ANSI.white}ipc${ANSI.reset} ${ANSI.cyan}det${ANSI.reset} ${ANSI.green}nd${ANSI.reset} ${ANSI.yellow}side${ANSI.reset} ${ANSI.dim}density░▄█${ANSI.reset}`,
  );

  return lines.join("\n");
}

function densityChar(count) {
  if (count <= 0) return " ";
  if (count === 1) return "░";
  if (count <= 3) return "▄";
  return "█";
}

function printablePreview(payload) {
  if (!payload || payload.length === 0) return null;
  const limit = Math.min(payload.length, 64);
  let text = "";
  for (let index = 0; index < limit; index += 1) {
    const value = payload[index];
    text += value >= 32 && value <= 126 ? String.fromCharCode(value) : ".";
  }
  return payload.length > limit ? `${text}…` : text;
}
