document.addEventListener("DOMContentLoaded", function () {
  // Project ID (sync scope). Default pfs25; override with ?project= in URL.
  const PROJECT_ID =
    new URLSearchParams(location.search).get("project") || "pfs25";

  // Avoid echo-posting when applying server state
  let applyingServer = false;

  // ------- Member name behavior (editable, propagate, persist) -------
  const names = document.querySelectorAll(
    '.member-name[contenteditable="true"]'
  );

  function updateRefs(key, text) {
    const refs = document.querySelectorAll('[data-ref-member="' + key + '"]');
    refs.forEach((r) => (r.textContent = text));
  }

  names.forEach((el) => {
    const key = el.getAttribute("data-member");
    const storageKey = `member-name-${PROJECT_ID}-` + key;

    try {
      const saved = localStorage.getItem(storageKey);
      if (saved) el.textContent = saved;
    } catch (e) {}

    updateRefs(key, el.textContent.trim());

    el.addEventListener("blur", () => {
      const text = el.textContent.trim() || "Person " + key;
      try {
        localStorage.setItem(storageKey, text);
      } catch (e) {}
      el.textContent = text;
      updateRefs(key, text);
    });

    el.addEventListener("input", () => {
      const temp = el.textContent.trim() || "Person " + key;
      updateRefs(key, temp);
    });

    el.addEventListener("keydown", (ev) => {
      if (ev.key === "Enter") {
        ev.preventDefault();
        el.blur();
      }
    });
  });

  // ------- Helpers for rings and colors -------
  function clampPercent(n) {
    return Math.min(100, Math.max(0, Math.round(Number(n) || 0)));
  }
  function colorFor(val) {
    if (val >= 80) return "#00b894";
    if (val >= 50) return "#f39c12";
    return "#e74c3c";
  }
  function setRingPercent(svgEl, percent) {
    const prog = svgEl.querySelector(".ring-progress");
    const r = parseFloat(prog.getAttribute("r")) || 24;
    const C = 2 * Math.PI * r;
    prog.style.strokeDasharray = String(C);
    const p = clampPercent(percent);
    prog.style.strokeDashoffset = String(C - (p / 100) * C);
    prog.style.stroke = colorFor(p);
    return p;
  }

  // ------- Server API helpers -------
  async function loadServerState(project) {
    try {
      const res = await fetch(
        `/.netlify/functions/state?project=${encodeURIComponent(project)}`,
        { cache: "no-store" }
      );
      if (!res.ok) {
        console.warn("Server state fetch failed:", res.status);
        return null;
      }
      return await res.json();
    } catch (err) {
      console.warn("Server state fetch error:", err);
      return null;
    }
  }

  function saveTaskToServer(project, taskId, isDone) {
    if (applyingServer) return;
    fetch(`/.netlify/functions/task?project=${encodeURIComponent(project)}`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ taskId, isDone: !!isDone }),
    }).catch((e) => console.warn("Task save error:", e));
  }

  function saveMetricsToServer(
    project,
    { tests_passed, tests_total, coverage }
  ) {
    if (applyingServer) return;
    fetch(
      `/.netlify/functions/metrics?project=${encodeURIComponent(project)}`,
      {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          tests_passed: Math.max(0, Math.round(Number(tests_passed) || 0)),
          tests_total: Math.max(0, Math.round(Number(tests_total) || 0)),
          coverage: clampPercent(coverage),
        }),
      }
    ).catch((e) => console.warn("Metrics save error:", e));
  }

  // ------- KPI: Tests Pass Rate -------
  (function initTests() {
    const chip = document.querySelector('.kpi-chip[data-kpi="tests"]');
    if (!chip) return;
    const ring = chip.querySelector(".micro-ring");
    const percentEl = chip.querySelector("[data-kpi-percent]");
    const passedInput = chip.querySelector(".input-passed");
    const totalInput = chip.querySelector(".input-total");

    const STORAGE = {
      passed: `kpi-${PROJECT_ID}-tests-passed`,
      total: `kpi-${PROJECT_ID}-tests-total`,
    };

    function update() {
      let p = Math.max(0, Math.round(Number(passedInput.value) || 0));
      let t = Math.max(0, Math.round(Number(totalInput.value) || 0));
      if (p > t) p = t;
      passedInput.value = p;
      totalInput.value = t;

      const percent = t === 0 ? 0 : Math.round((p / t) * 100);
      const pv = setRingPercent(ring, percent);
      percentEl.textContent = pv + "%";
      return { p, t };
    }

    function persistLocal(p, t) {
      try {
        localStorage.setItem(STORAGE.passed, String(p));
        localStorage.setItem(STORAGE.total, String(t));
      } catch (e) {}
    }

    function persistServer(p, t) {
      const coverage = getCoverageValue();
      saveMetricsToServer(PROJECT_ID, {
        tests_passed: p,
        tests_total: t,
        coverage,
      });
    }

    // Load local immediately (offline), will be overridden by server if present
    try {
      const sp = localStorage.getItem(STORAGE.passed);
      const st = localStorage.getItem(STORAGE.total);
      passedInput.value =
        sp !== null ? Math.max(0, Math.round(Number(sp) || 0)) : 0;
      totalInput.value =
        st !== null ? Math.max(0, Math.round(Number(st) || 0)) : 0;
    } catch {}
    update();

    ["input"].forEach((evt) => {
      passedInput.addEventListener(evt, () => {
        update();
      });
    });
    ["input"].forEach((evt) => {
      totalInput.addEventListener(evt, () => {
        update();
      });
    });
    ["change", "blur"].forEach((evt) => {
      passedInput.addEventListener(evt, () => {
        const { p, t } = update();
        persistLocal(p, t);
        persistServer(p, t);
      });
      totalInput.addEventListener(evt, () => {
        const { p, t } = update();
        persistLocal(p, t);
        persistServer(p, t);
      });
    });
    [passedInput, totalInput].forEach((inp) => {
      inp.addEventListener("keydown", (e) => {
        if (e.key === "Enter") {
          e.preventDefault();
          const { p, t } = update();
          persistLocal(p, t);
          persistServer(p, t);
          inp.blur();
        }
      });
    });

    chip.__getValues = () => {
      const p = Math.max(0, Math.round(Number(passedInput.value) || 0));
      const t = Math.max(0, Math.round(Number(totalInput.value) || 0));
      return { p: Math.min(p, t), t };
    };
    chip.__applyValues = ({ p, t }) => {
      applyingServer = true;
      passedInput.value = Math.max(0, Math.round(p || 0));
      totalInput.value = Math.max(0, Math.round(t || 0));
      update();
      persistLocal(passedInput.value, totalInput.value);
      applyingServer = false;
    };
  })();

  function getTestsValues() {
    const chip = document.querySelector('.kpi-chip[data-kpi="tests"]');
    if (!chip) return { p: 0, t: 0 };
    return chip.__getValues ? chip.__getValues() : { p: 0, t: 0 };
  }

  // ------- KPI: Code Coverage -------
  (function initCoverage() {
    const chip = document.querySelector('.kpi-chip[data-kpi="coverage"]');
    if (!chip) return;
    const ring = chip.querySelector(".micro-ring");
    const percentEl = chip.querySelector("[data-kpi-percent]");
    const slider = chip.querySelector(".coverage-range");
    const number = chip.querySelector(".coverage-number");

    const STORAGE_KEY = `kpi-${PROJECT_ID}-coverage`;

    const apply = (v) => {
      const val = clampPercent(v);
      slider.value = val;
      number.value = val;
      const p = setRingPercent(ring, val);
      percentEl.textContent = p + "%";
      return val;
    };

    try {
      const s = localStorage.getItem(STORAGE_KEY);
      apply(s !== null ? s : 0);
    } catch {
      apply(0);
    }

    const syncFromSlider = () => apply(slider.value);
    const syncFromNumber = () => apply(number.value);

    const persistLocal = (v) => {
      try {
        localStorage.setItem(STORAGE_KEY, String(clampPercent(v)));
      } catch (e) {}
    };
    const persistServer = (v) => {
      const { p, t } = getTestsValues();
      saveMetricsToServer(PROJECT_ID, {
        tests_passed: p,
        tests_total: t,
        coverage: clampPercent(v),
      });
    };

    slider.addEventListener("input", () => {
      syncFromSlider();
    });
    slider.addEventListener("change", () => {
      const v = syncFromSlider();
      persistLocal(v);
      persistServer(v);
    });
    number.addEventListener("input", () => {
      syncFromNumber();
    });
    number.addEventListener("change", () => {
      const v = syncFromNumber();
      persistLocal(v);
      persistServer(v);
    });
    number.addEventListener("keydown", (e) => {
      if (e.key === "Enter") {
        e.preventDefault();
        const v = syncFromNumber();
        persistLocal(v);
        persistServer(v);
        number.blur();
      }
    });

    chip.__getValue = () => clampPercent(number.value);
    chip.__applyValue = (v) => {
      applyingServer = true;
      const val = apply(v);
      persistLocal(val);
      applyingServer = false;
    };
  })();

  function getCoverageValue() {
    const chip = document.querySelector('.kpi-chip[data-kpi="coverage"]');
    if (!chip) return 0;
    return chip.__getValue ? chip.__getValue() : 0;
  }

  // ------- Timeline Tasks: Done toggles + Project Completion + Team Summary -------
  (function initTimelineTasks() {
    const completionChip = document.querySelector(
      '.kpi-chip[data-kpi="completion"]'
    );
    const completionRing = completionChip
      ? completionChip.querySelector(".micro-ring")
      : null;
    const completionPercentEl = completionChip
      ? completionChip.querySelector("[data-kpi-percent]")
      : null;

    function updateCompletion(percent) {
      if (!completionRing || !completionPercentEl) return;
      const p = setRingPercent(completionRing, percent);
      completionPercentEl.textContent = p + "%";
    }

    function taskStorageKey(id) {
      return `timeline-task-done-${PROJECT_ID}-${id}`;
    }

    const localState = {};
    const weeks = Array.from(
      document.querySelectorAll(".timeline .week-container")
    );
    weeks.forEach((weekEl, wi) => {
      const tasks = Array.from(
        weekEl.querySelectorAll(".task-grid .task-card")
      );
      tasks.forEach((taskEl, ti) => {
        const id = `w${wi + 1}-t${ti + 1}`;
        taskEl.setAttribute("data-task-id", id);

        if (!taskEl.querySelector(".task-done-toggle")) {
          const toggle = document.createElement("div");
          toggle.className = "task-done-toggle";
          const cb = document.createElement("input");
          cb.type = "checkbox";
          cb.className = "task-done-checkbox";
          cb.id = `task-${id}`;
          cb.setAttribute("aria-label", "Mark task as done");
          const lbl = document.createElement("label");
          lbl.className = "task-done-label";
          lbl.htmlFor = cb.id;
          lbl.textContent = "Done";

          toggle.appendChild(cb);
          toggle.appendChild(lbl);
          taskEl.appendChild(toggle);

          try {
            const saved = localStorage.getItem(taskStorageKey(id));
            const checked = saved === "1";
            cb.checked = checked;
            if (checked) taskEl.classList.add("is-done");
            localState[id] = checked;
          } catch {}

          cb.addEventListener("change", () => {
            const checked = cb.checked;
            taskEl.classList.toggle("is-done", checked);
            try {
              localStorage.setItem(taskStorageKey(id), checked ? "1" : "0");
            } catch (e) {}
            saveTaskToServer(PROJECT_ID, id, checked);
            recalcAll();
          });
        }
      });
    });

    function recalcAll() {
      const allTasks = Array.from(
        document.querySelectorAll(".timeline .task-card[data-task-id]")
      );
      const total = allTasks.length;
      const done = allTasks.reduce(
        (acc, el) => acc + (el.classList.contains("is-done") ? 1 : 0),
        0
      );
      const overallPct = total === 0 ? 0 : Math.round((done / total) * 100);
      updateCompletion(overallPct);

      const persons = ["A", "B", "C", "D", "E", "F"];
      const stats = {};
      persons.forEach((p) => (stats[p] = { done: 0, total: 0 }));

      allTasks.forEach((task) => {
        const refs = Array.from(
          task.querySelectorAll(".ref-member[data-ref-member]")
        )
          .map((el) => (el.getAttribute("data-ref-member") || "").trim())
          .filter((code) => /^[A-F]$/.test(code));
        if (refs.length === 0) return;
        const isDone = task.classList.contains("is-done");
        refs.forEach((code) => {
          if (!stats[code]) stats[code] = { done: 0, total: 0 };
          stats[code].total += 1;
          if (isDone) stats[code].done += 1;
        });
      });

      persons.forEach((code) => {
        const chip = document.querySelector(
          `.summary-chip[data-person="${code}"]`
        );
        if (!chip) return;
        const ring = chip.querySelector(".nano-ring");
        const pctEl = chip.querySelector("[data-summary-percent]");
        const { done, total } = stats[code];
        const pct = total === 0 ? 0 : Math.round((done / total) * 100);
        const p = setRingPercent(ring, pct);
        pctEl.textContent = p + "%";
      });
    }

    recalcAll();

    (async () => {
      applyingServer = true;
      const data = await loadServerState(PROJECT_ID);
      if (data) {
        const serverTasks = data.tasks || {};
        const serverHasTasks = Object.keys(serverTasks).length > 0;

        if (serverHasTasks) {
          Object.entries(serverTasks).forEach(([id, isDone]) => {
            const cb = document.querySelector(
              `.task-card[data-task-id="${id}"] .task-done-checkbox`
            );
            const taskEl = document.querySelector(
              `.task-card[data-task-id="${id}"]`
            );
            if (cb && taskEl) {
              cb.checked = !!isDone;
              taskEl.classList.toggle("is-done", !!isDone);
              try {
                localStorage.setItem(taskStorageKey(id), isDone ? "1" : "0");
              } catch (e) {}
            }
          });
        } else {
          Object.entries(localState).forEach(([id, checked]) => {
            saveTaskToServer(PROJECT_ID, id, !!checked);
          });
        }

        const testsChip = document.querySelector('.kpi-chip[data-kpi="tests"]');
        const covChip = document.querySelector(
          '.kpi-chip[data-kpi="coverage"]'
        );
        const metrics = data.metrics || null;

        if (metrics) {
          const { tests_passed = 0, tests_total = 0, coverage = 0 } = metrics;
          if (testsChip && testsChip.__applyValues) {
            testsChip.__applyValues({ p: tests_passed, t: tests_total });
          }
          if (covChip && covChip.__applyValue) {
            covChip.__applyValue(coverage);
          }
        } else {
          if (testsChip && testsChip.__getValues) {
            const { p, t } = testsChip.__getValues();
            const cov = getCoverageValue();
            saveMetricsToServer(PROJECT_ID, {
              tests_passed: p,
              tests_total: t,
              coverage: cov,
            });
          }
        }

        recalcAll();
      } else {
        console.warn(
          "Working offline (server unreachable). Changes will be local only."
        );
      }
      applyingServer = false;
    })();
  })();

  document.querySelectorAll(".member-name").forEach((el) => {
    el.addEventListener("focus", () => el.classList.add("focused"));
    el.addEventListener("blur", () => el.classList.remove("focused"));
  });
});
