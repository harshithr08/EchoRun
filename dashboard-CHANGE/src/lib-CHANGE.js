export const laneColors = {
  signals: "bg-fuchsia-400",
  process: "bg-blue-500",
  deterministic: "bg-sky-400",
  non_deterministic: "bg-emerald-400",
  side_effect: "bg-amber-400",
};

export function mergeEventRows(recordEvents = [], replayEvents = []) {
  const sequences = [...new Set([...recordEvents.map((event) => event.seq), ...replayEvents.map((event) => event.seq)])].sort(
    (left, right) => left - right,
  );

  const recordBySeq = new Map(recordEvents.map((event) => [event.seq, event]));
  const replayBySeq = new Map(replayEvents.map((event) => [event.seq, event]));

  return sequences.map((seq) => {
    const record = recordBySeq.get(seq) ?? null;
    const replay = replayBySeq.get(seq) ?? null;
    const diverged = Boolean(record?.diverged || replay?.diverged);

    return { seq, record, replay, diverged };
  });
}

export function selectTimelineEvents(artifact) {
  if (!artifact) return [];
  if (artifact.record?.events?.length) return artifact.record.events;
  if (artifact.replay?.events?.length) return artifact.replay.events;
  return [];
}

export function formatValue(value) {
  if (value === null || value === undefined) return "n/a";
  return String(value);
}

export function detectLogStyle(line) {
  if (/DIVERGENCE|DIVERGED|expected .* got/i.test(line)) return "danger";
  if (/error|failed|cannot|no such/i.test(line)) return "warning";
  return "normal";
}
