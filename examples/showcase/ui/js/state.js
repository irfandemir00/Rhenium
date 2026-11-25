const STORAGE_KEY = "rhenium_showcase_todos_v1";

function loadTodos() {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (raw) {
      const parsed = JSON.parse(raw);
      if (Array.isArray(parsed) && parsed.length > 0) {
        return parsed;
      }
    }
  } catch (_) {}
  return [
    { id: 1, title: "C++ kabuğunu başlat", done: true },
    { id: 2, title: "Tüm uygulama fonksiyonlarını JS ile yaz", done: false }
  ];
}

export const state = {
  counter: 0,
  greetingName: "Geliştirici",
  showTips: true,
  highContrast: false,
  taskRunning: false,
  taskProgress: 0,
  taskStateLabel: "Hazır",
  todos: loadTodos()
};

export function saveTodos() {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(state.todos));
  } catch (_) {}
}

export function todoSummaryText() {
  const doneCount = state.todos.filter((t) => t.done).length;
  return `${state.todos.length} görev (${doneCount} tamamlandı)`;
}
