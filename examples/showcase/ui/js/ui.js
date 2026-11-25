import { saveTodos, state, todoSummaryText } from "./state.js";

export function $(id) {
  return document.getElementById(id);
}

export function setOutput(text) {
  $("native-output").textContent = text;
}

export function renderCounter() {
  const badge = `Sayaç: ${state.counter}`;
  $("counter-display").textContent = String(state.counter);
  $("kpi-counter").textContent = String(state.counter);
  $("kpi-counter-meta").textContent = badge;
  $("panel-counter-pill").textContent = badge;
}

export function renderGreeting() {
  const name = state.greetingName.trim();
  $("greeting-preview").textContent = name
    ? `Merhaba, ${name}! (JavaScript)`
    : "İsim bekleniyor...";
}

export function renderTodos() {
  const list = $("todo-list");
  list.replaceChildren();

  const summary = todoSummaryText();
  $("kpi-todo-count").textContent = String(state.todos.length);
  $("kpi-todo-meta").textContent = summary;
  $("panel-todo-pill").textContent = summary;

  if (state.todos.length === 0) {
    const empty = document.createElement("li");
    empty.className = "todo-empty";
    empty.textContent = "Henüz görev yok. Yukarıdaki alandan ekleyin.";
    list.appendChild(empty);
    return;
  }

  for (const item of state.todos) {
    const li = document.createElement("li");
    li.className = item.done ? "todo-item done" : "todo-item";

    const left = document.createElement("div");
    left.className = "todo-left";

    const toggleBtn = document.createElement("button");
    toggleBtn.type = "button";
    toggleBtn.className = "todo-check";
    toggleBtn.setAttribute("aria-label", "Görevi tamamlandı olarak işaretle");
    toggleBtn.textContent = item.done ? "✓" : "○";
    toggleBtn.addEventListener("click", () => {
      item.done = !item.done;
      saveTodos();
      renderTodos();
    });

    const title = document.createElement("span");
    title.className = "todo-title";
    title.textContent = item.title;

    left.append(toggleBtn, title);

    const delBtn = document.createElement("button");
    delBtn.type = "button";
    delBtn.className = "btn btn-danger-ghost";
    delBtn.setAttribute("aria-label", "Görevi sil");
    delBtn.textContent = "Sil";
    delBtn.addEventListener("click", () => {
      state.todos = state.todos.filter((t) => t.id !== item.id);
      saveTodos();
      renderTodos();
    });

    li.append(left, delBtn);
    list.appendChild(li);
  }
}

export function renderTask() {
  const pct = `${state.taskProgress}%`;
  $("progress-fill").style.width = pct;
  $("progress-label").textContent = pct;
  $("kpi-task-percent").textContent = pct;
  $("kpi-task-state").textContent = state.taskStateLabel;
  $("sidebar-task-state").textContent = state.taskStateLabel;
  $("task-badge-idle").textContent = state.taskStateLabel;
  $("task-badge-running").hidden = !state.taskRunning;
  $("task-badge-idle").hidden = state.taskRunning;
  $("btn-benchmark").disabled = state.taskRunning;
}
