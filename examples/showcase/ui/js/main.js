import { saveTodos, state } from "./state.js";
import { $, renderCounter, renderGreeting, renderTask, renderTodos } from "./ui.js";
import {
  handleCopySummary,
  handleListFolder,
  handleMessageBox,
  handleOpenFile,
  handleReadClipboard,
  handleSaveReport,
  loadRuntimeInfo,
  wireWindowControls
} from "./native.js";

async function runAsyncBenchmark() {
  if (state.taskRunning) return;
  state.taskRunning = true;
  state.taskStateLabel = "Çalışıyor";
  $("task-status").textContent = "JavaScript asenkron görev çalışıyor...";
  renderTask();

  for (let pct = 0; pct <= 100; pct += 10) {
    state.taskProgress = pct;
    renderTask();
    await new Promise((resolve) => setTimeout(resolve, 120));
  }

  state.taskRunning = false;
  state.taskStateLabel = "Tamamlandı";
  $("task-status").textContent = "Tamamlandı! Tüm döngü JavaScript ile yürütüldü.";
  renderTask();
}

function wireAppEvents() {
  wireWindowControls();

  $("btn-inc").addEventListener("click", () => {
    state.counter += 1;
    renderCounter();
  });
  $("btn-dec").addEventListener("click", () => {
    state.counter -= 1;
    renderCounter();
  });
  $("btn-reset").addEventListener("click", () => {
    state.counter = 0;
    renderCounter();
  });

  $("greeting-input").addEventListener("input", (e) => {
    state.greetingName = e.target.value;
    renderGreeting();
  });

  $("todo-form").addEventListener("submit", (e) => {
    e.preventDefault();
    const input = $("todo-title-input");
    const title = input.value.trim();
    if (!title) return;
    const nextId = state.todos.reduce((max, t) => Math.max(max, t.id), 0) + 1;
    state.todos.push({ id: nextId, title, done: false });
    saveTodos();
    renderTodos();
    input.value = "";
  });

  $("btn-benchmark").addEventListener("click", runAsyncBenchmark);

  $("toggle-contrast").addEventListener("change", (e) => {
    state.highContrast = e.target.checked;
    document.body.classList.toggle("high-contrast-accent", state.highContrast);
  });

  $("btn-toggle-tips").addEventListener("click", () => {
    state.showTips = !state.showTips;
    $("tips-banner").hidden = !state.showTips;
  });

  $("btn-open-file").addEventListener("click", handleOpenFile);
  $("side-open-file").addEventListener("click", handleOpenFile);

  $("btn-open-folder").addEventListener("click", handleListFolder);
  $("side-scan-folder").addEventListener("click", handleListFolder);

  $("btn-save-report").addEventListener("click", handleSaveReport);
  $("side-save-report").addEventListener("click", handleSaveReport);

  $("btn-copy-summary").addEventListener("click", handleCopySummary);
  $("side-copy-summary").addEventListener("click", handleCopySummary);

  $("btn-read-clipboard").addEventListener("click", handleReadClipboard);

  $("btn-native-alert").addEventListener("click", handleMessageBox);
  $("side-alert").addEventListener("click", handleMessageBox);
}

wireAppEvents();
renderCounter();
renderGreeting();
renderTodos();
renderTask();
loadRuntimeInfo();
