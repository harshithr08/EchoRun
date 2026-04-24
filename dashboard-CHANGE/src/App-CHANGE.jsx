import { memo, useCallback, useEffect, useMemo, useRef, useState } from "react";

import { detectLogStyle, formatValue, laneColors, mergeEventRows, selectTimelineEvents } from "./lib-CHANGE.js";

const SCROLLABLE_SIDES = ["record", "replay"];
const EVENT_ROW_HEIGHT = 52;

function loadJson(path) {
  return fetch(path, { cache: "no-store" }).then(async (response) => {
    if (!response.ok) {
      throw new Error(`Failed to load ${path}: ${response.status}`);
    }
    return response.json();
  });
}

export default function App() {
  const [manifest, setManifest] = useState(null);
  const [manifestError, setManifestError] = useState(null);
  const [selectedSlug, setSelectedSlug] = useState(null);
  const [artifact, setArtifact] = useState(null);
  const [artifactError, setArtifactError] = useState(null);
  const [selectedSeq, setSelectedSeq] = useState(null);
  const [activeLogTab, setActiveLogTab] = useState("record");
  const columnRefs = useRef({ record: null, replay: null });
  const pendingScrollBehavior = useRef(null);

  useEffect(() => {
    loadJson("/out/manifest.json")
      .then((data) => {
        setManifest(data);
        setSelectedSlug((current) => current ?? data.targets?.[0]?.slug ?? null);
      })
      .catch((error) => {
        setManifest(null);
        setManifestError(error.message);
      });
  }, []);

  useEffect(() => {
    if (!manifest) return;
    if (!selectedSlug) return;

    const target = manifest.targets.find((entry) => entry.slug === selectedSlug);
    if (!target) return;

    setArtifactError(null);
    loadJson(target.artifactPath)
      .then((data) => {
        setArtifact(data);
        const preferredSeq = data.divergence?.seq ?? selectTimelineEvents(data)[0]?.seq ?? 0;
        pendingScrollBehavior.current = "auto";
        setSelectedSeq(preferredSeq);
        setActiveLogTab("record");
      })
      .catch((error) => {
        setArtifact(null);
        setArtifactError(error.message);
      });
  }, [manifest, selectedSlug]);

  const rows = useMemo(
    () => mergeEventRows(artifact?.record?.events ?? [], artifact?.replay?.events ?? []),
    [artifact?.record?.events, artifact?.replay?.events],
  );
  const rowIndexBySeq = useMemo(() => new Map(rows.map((row, index) => [row.seq, index])), [rows]);
  const timelineEvents = useMemo(() => selectTimelineEvents(artifact), [artifact]);
  const selectedRow = useMemo(() => rows.find((row) => row.seq === selectedSeq) ?? rows[0] ?? null, [rows, selectedSeq]);

  const logTabs = useMemo(() => {
    const tabs = [];
    if (artifact?.record?.log) tabs.push({ id: "record", label: "record", content: artifact.record.log });
    if (artifact?.replay?.log) tabs.push({ id: "replay", label: "replay", content: artifact.replay.log });
    if (artifact?.repl?.log) tabs.push({ id: "repl", label: "repl", content: artifact.repl.log });
    return tabs;
  }, [artifact]);

  useEffect(() => {
    if (logTabs.length === 0) return;
    if (!logTabs.some((tab) => tab.id === activeLogTab)) {
      setActiveLogTab(logTabs[0].id);
    }
  }, [activeLogTab, logTabs]);

  useEffect(() => {
    const behavior = pendingScrollBehavior.current;
    if (selectedSeq === null || selectedSeq === undefined || !behavior) return;

    pendingScrollBehavior.current = null;

    const frameId = window.requestAnimationFrame(() => {
      const rowIndex = rowIndexBySeq.get(selectedSeq);
      if (rowIndex === undefined) return;

      SCROLLABLE_SIDES.forEach((side) => {
        const node = columnRefs.current[side];
        if (!node) return;

        const maxTop = Math.max(0, node.scrollHeight - node.clientHeight);
        const nextTop = Math.min(maxTop, Math.max(0, rowIndex * EVENT_ROW_HEIGHT - (node.clientHeight - EVENT_ROW_HEIGHT) / 2));

        if (behavior === "auto") {
          node.scrollTop = nextTop;
          return;
        }

        node.scrollTo({ top: nextTop, behavior });
      });
    });

    return () => window.cancelAnimationFrame(frameId);
  }, [rowIndexBySeq, selectedSeq]);

  const registerColumnRef = useCallback((side, node) => {
    columnRefs.current[side] = node;
  }, []);

  const scrollColumnsToSeq = useCallback(
    (seq, behavior = "auto") => {
      const rowIndex = rowIndexBySeq.get(seq);
      if (rowIndex === undefined) return;

      SCROLLABLE_SIDES.forEach((side) => {
        const node = columnRefs.current[side];
        if (!node) return;

        const maxTop = Math.max(0, node.scrollHeight - node.clientHeight);
        const nextTop = Math.min(maxTop, Math.max(0, rowIndex * EVENT_ROW_HEIGHT - (node.clientHeight - EVENT_ROW_HEIGHT) / 2));

        if (behavior === "auto") {
          node.scrollTop = nextTop;
          return;
        }

        node.scrollTo({ top: nextTop, behavior });
      });
    },
    [rowIndexBySeq],
  );

  const handleSeqSelect = useCallback((seq, options = {}) => {
    const { scroll = false, behavior = "auto" } = options;

    if (scroll) {
      pendingScrollBehavior.current = behavior;
    }

    setSelectedSeq((current) => {
      if (current === seq) {
        if (scroll) {
          window.requestAnimationFrame(() => {
            scrollColumnsToSeq(seq, behavior);
          });
        }
        return current;
      }

      return seq;
    });
  }, [scrollColumnsToSeq]);

  const activeTarget = useMemo(() => {
    const targets = manifest?.targets ?? [];
    return targets.find((entry) => entry.slug === selectedSlug) ?? targets[0] ?? null;
  }, [manifest, selectedSlug]);

  const activeLog = useMemo(() => logTabs.find((tab) => tab.id === activeLogTab) ?? logTabs[0] ?? null, [logTabs, activeLogTab]);
  const activeLogLines = useMemo(
    () =>
      (activeLog?.content ?? "").split("\n").map((line, index) => ({
        key: `${activeLog?.id ?? "log"}-${index}`,
        tone: detectLogStyle(line),
        line,
      })),
    [activeLog],
  );
  const inspectorRecord = selectedRow?.record ?? null;
  const inspectorReplay = selectedRow?.replay ?? null;
  const focusEvent = inspectorReplay ?? inspectorRecord;

  if (manifestError || !manifest || (manifest.targets ?? []).length === 0) {
    return (
      <main className="min-h-screen bg-mist px-6 py-10 font-mono text-ink">
        <div className="mx-auto max-w-5xl border border-border bg-paper/90 p-10 shadow-panel backdrop-blur animate-rise">
          <div className="mb-8 inline-flex items-center gap-3 border border-blue-200 bg-blue-50 px-4 py-2 text-xs uppercase tracking-[0.32em] text-blue-700">
            EchoRun Review Dashboard
          </div>
          <h1 className="mb-3 text-4xl font-semibold tracking-tight">Artifacts not found</h1>
          <p className="max-w-2xl text-sm leading-7 text-calm">
            This dashboard is static and only reads generated review artifacts from <code>/out</code>. Run{" "}
            <code>make test</code> at the project root, then refresh this page.
          </p>
          {manifestError ? (
            <div className="mt-8 border border-rose-200 bg-rose-50 px-5 py-4 text-sm text-rose-700">
              {manifestError}
            </div>
          ) : null}
        </div>
      </main>
    );
  }

  return (
    <main className="dashboard-shell min-h-screen px-4 py-4 font-mono text-ink md:px-6 md:py-6">
      <div className="mx-auto grid max-w-[1600px] gap-4 xl:grid-cols-[280px_minmax(0,1fr)]">
        <aside className="panel-shell overflow-hidden">
          <div className="border-b border-border bg-white/55 px-6 py-6">
            <div className="mb-3 inline-flex h-10 w-10 items-center justify-center border border-blue-100 bg-blue-50 text-xl text-accent">
              ⛁
            </div>
            <h1 className="text-2xl font-semibold tracking-tight">EchoRun</h1>
            <p className="mt-2 text-xs uppercase tracking-[0.3em] text-calm">Review Targets</p>
          </div>

          <div className="space-y-2 p-3">
            {manifest.targets.map((target, index) => {
              const selected = target.slug === activeTarget.slug;
              return (
                <button
                  key={target.slug}
                  type="button"
                  onClick={() => setSelectedSlug(target.slug)}
                  className={`w-full border px-4 py-4 text-left transition-colors duration-150 ${
                    selected
                      ? "border-blue-500 bg-accent text-white shadow-lg shadow-blue-200/50"
                      : "border-border/60 bg-slate-50/90 text-ink hover:border-blue-200 hover:bg-white"
                  }`}
                >
                  <div className="mb-2 text-[11px] uppercase tracking-[0.3em] opacity-70">target {index + 1}</div>
                  <div className="text-lg font-semibold">{target.label}</div>
                  <div className={`mt-2 text-xs uppercase tracking-[0.24em] ${selected ? "text-blue-100" : "text-calm"}`}>
                    {target.eventCount} events
                  </div>
                  {target.diverged ? (
                    <div className={`mt-3 inline-flex border px-3 py-1 text-[11px] uppercase tracking-[0.24em] ${
                      selected ? "border-white/20 bg-white/10 text-white" : "border-rose-200 bg-rose-100 text-rose-700"
                    }`}>
                      divergence
                    </div>
                  ) : null}
                </button>
              );
            })}
          </div>
        </aside>

        <section className="space-y-4">
          <header className="panel-shell overflow-hidden">
            <div className="border-b border-border bg-white/55 px-6 py-6">
              <div className="text-[11px] uppercase tracking-[0.32em] text-calm">selected command</div>
              <div className="mt-3 text-3xl font-semibold tracking-tight">{activeTarget.label}</div>
              <div className="mt-2 text-sm text-calm">{artifact?.command}</div>
            </div>

            <div className="px-6 py-5">
              <div className="mb-4 flex items-center justify-between gap-4">
                <div>
                  <div className="text-[11px] uppercase tracking-[0.32em] text-calm">timeline execution strip</div>
                  <div className="mt-2 text-xs text-calm">Select a strip to sync the matching record and replay rows.</div>
                </div>
                <div className="flex flex-wrap gap-3 text-[11px] uppercase tracking-[0.24em] text-calm">
                  {Object.entries(laneColors).map(([lane, color]) => (
                    <div key={lane} className="inline-flex items-center gap-2">
                      <span className={`inline-block h-2.5 w-2.5 ${color}`} />
                      {lane.replaceAll("_", " ")}
                    </div>
                  ))}
                </div>
              </div>

              <div className="panel-scroll overflow-x-auto border border-border/70 bg-white/50 p-3 pb-1">
                <TimelineStrip
                  artifact={artifact}
                  selectedSeq={selectedSeq}
                  timelineEvents={timelineEvents}
                  onSelect={handleSeqSelect}
                />
              </div>
            </div>
          </header>

          {artifactError ? (
            <div className="panel-shell border-rose-200 bg-rose-50 px-5 py-4 text-sm text-rose-700">{artifactError}</div>
          ) : null}

          <div className="grid gap-4 xl:grid-cols-[minmax(0,1.45fr)_420px]">
            <section className="panel-shell overflow-hidden">
              <div className="grid border-b border-border bg-slate-100/80 text-[11px] uppercase tracking-[0.28em] text-calm md:grid-cols-2">
                <div className="border-r border-border px-6 py-4">record path</div>
                <div className="px-6 py-4">replay path</div>
              </div>

              <div className="grid md:grid-cols-2">
                <EventColumn
                  rows={rows}
                  side="record"
                  selectedSeq={selectedSeq}
                  onSelect={handleSeqSelect}
                  registerColumnRef={registerColumnRef}
                />
                <EventColumn
                  rows={rows}
                  side="replay"
                  selectedSeq={selectedSeq}
                  onSelect={handleSeqSelect}
                  registerColumnRef={registerColumnRef}
                />
              </div>
            </section>

            <div className="space-y-4">
              <section className="panel-shell overflow-hidden">
                <div className="border-b border-border bg-white/55 px-6 py-4 text-[11px] uppercase tracking-[0.28em] text-calm">
                  payload &amp; arg inspector
                </div>
                <div className="space-y-5 px-6 py-5">
                  {focusEvent ? (
                    <>
                      <div className="grid grid-cols-2 gap-3">
                        <InspectorField label="sequence" value={focusEvent.seq} />
                        <InspectorField label="trace seq" value={formatValue(focusEvent.traceSeq)} />
                        <InspectorField label="lane" value={focusEvent.lane} />
                        <InspectorField label="category" value={focusEvent.category} />
                        <InspectorField label="syscall id" value={focusEvent.syscallNo} />
                        <InspectorField label="syscall" value={focusEvent.syscallName} />
                        <InspectorField label="retval" value={formatValue(focusEvent.retval)} />
                        <InspectorField label="payload" value={`${focusEvent.payloadSize ?? 0} bytes`} />
                      </div>

                      {artifact?.divergence && artifact.divergence.seq === focusEvent.seq ? (
                        <div className="border border-rose-200 bg-rose-50 px-4 py-4 text-sm text-rose-700">
                          {artifact.divergence.message}
                        </div>
                      ) : null}

                      <div className="border border-border bg-slate-50/80 p-4">
                        <div className="mb-3 text-[11px] uppercase tracking-[0.28em] text-calm">payload preview</div>
                        <pre className="panel-scroll overflow-x-auto whitespace-pre-wrap text-sm leading-7 text-ink">
                          {focusEvent.payloadPreview ?? "No captured payload for this event."}
                        </pre>
                      </div>
                    </>
                  ) : (
                    <div className="flex min-h-[280px] items-center justify-center border border-dashed border-border bg-slate-50/80 text-sm text-calm">
                      Select a timeline block or row to inspect its metadata.
                    </div>
                  )}
                </div>
              </section>

              <LogPanel
                activeLogLines={activeLogLines}
                activeLogTab={activeLogTab}
                logTabs={logTabs}
                onTabChange={setActiveLogTab}
              />
            </div>
          </div>
        </section>
      </div>
    </main>
  );
}

const TimelineStrip = memo(function TimelineStrip({ artifact, selectedSeq, timelineEvents, onSelect }) {
  return (
    <div className="inline-flex min-w-full gap-1">
      {timelineEvents.map((event) => (
        <TimelineEventButton
          key={`${event.seq}-${event.lane}`}
          diverged={event.diverged || artifact?.divergence?.seq === event.seq}
          event={event}
          onSelect={onSelect}
          selected={event.seq === selectedSeq}
        />
      ))}
    </div>
  );
});

const TimelineEventButton = memo(function TimelineEventButton({ diverged, event, onSelect, selected }) {
  return (
    <button
      type="button"
      onClick={() => onSelect(event.seq, { scroll: true, behavior: "auto" })}
      className={`relative h-16 w-5 shrink-0 border border-white/70 transition-colors duration-150 ${
        laneColors[event.lane] ?? "bg-slate-300"
      } ${selected ? "border-blue-600 outline outline-1 outline-blue-500" : "hover:border-slate-300"}`}
      title={`#${event.seq} ${event.eventName}`}
    >
      {diverged ? <span className="absolute inset-y-0 left-1/2 w-[2px] -translate-x-1/2 bg-red-600" /> : null}
    </button>
  );
});

const EventColumn = memo(function EventColumn({ rows, side, selectedSeq, onSelect, registerColumnRef }) {
  const setContainerRef = useCallback(
    (node) => {
      registerColumnRef(side, node);
    },
    [registerColumnRef, side],
  );

  return (
    <div
      ref={setContainerRef}
      className="panel-scroll max-h-[760px] overflow-y-auto border-r border-border last:border-r-0"
    >
      {rows.map((row) => (
        <EventRow
          key={`${side}-${row.seq}`}
          onSelect={onSelect}
          row={row}
          selected={row.seq === selectedSeq}
          side={side}
        />
      ))}
    </div>
  );
});

const EventRow = memo(function EventRow({ onSelect, row, selected, side }) {
  const event = row[side];

  return (
    <button
      type="button"
      onClick={() => onSelect(row.seq)}
      className={`grid h-[52px] w-full grid-cols-[84px_minmax(0,1fr)_auto] items-center gap-3 border-b border-slate-100 px-5 text-left text-sm transition-colors duration-150 ${
        selected
          ? "bg-blue-50/80 shadow-[inset_3px_0_0_0_rgba(47,109,246,1)]"
          : "hover:bg-slate-50/90"
      } ${row.diverged ? "bg-rose-50/80" : ""}`}
    >
      <span className="text-calm">#{row.seq}</span>
      <span className={event ? "text-ink" : "text-slate-300"}>{event?.eventName ?? "—"}</span>
      <span className="text-xs uppercase tracking-[0.2em] text-calm">{event?.payloadSize ? `pay ${event.payloadSize}` : ""}</span>
    </button>
  );
});

const LogPanel = memo(function LogPanel({ activeLogLines, activeLogTab, logTabs, onTabChange }) {
  return (
    <section className="panel-shell overflow-hidden">
      <div className="flex border-b border-border bg-slate-950 text-[11px] uppercase tracking-[0.28em]">
        {logTabs.map((tab) => (
          <button
            key={tab.id}
            type="button"
            onClick={() => onTabChange(tab.id)}
            className={`px-5 py-4 transition-colors duration-150 ${
              tab.id === activeLogTab ? "border-b-2 border-accent text-white" : "text-slate-500 hover:text-slate-300"
            }`}
          >
            {tab.label}
          </button>
        ))}
      </div>

      <div className="bg-slate-950 px-5 py-4 text-sm text-slate-200">
        <pre className="panel-scroll max-h-[360px] overflow-auto whitespace-pre-wrap leading-7">
          {activeLogLines.map(({ key, line, tone }) => (
            <div key={key} className={tone === "danger" ? "text-rose-300" : tone === "warning" ? "text-amber-300" : ""}>
              {line}
            </div>
          ))}
        </pre>
      </div>
    </section>
  );
});

function InspectorField({ label, value }) {
  return (
    <div className="border border-border bg-slate-50/80 px-4 py-3">
      <div className="text-[11px] uppercase tracking-[0.28em] text-calm">{label}</div>
      <div className="mt-2 text-sm text-ink">{value}</div>
    </div>
  );
}
