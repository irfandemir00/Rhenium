import { state, todoSummaryText } from "./state.js";
import { $, setOutput } from "./ui.js";

const rh = window.rhenium;

export function wireWindowControls() {
  $("btn-devtools").addEventListener("click", () => rh.window.openDevTools());
  $("btn-minimize").addEventListener("click", () => rh.window.minimize());
  $("btn-maximize").addEventListener("click", () => rh.window.toggleMaximize());
  $("btn-close").addEventListener("click", () => rh.window.close());
}

export async function handleOpenFile() {
  const path = await rh.dialog.openFile({
    title: "Okunacak Dosyayı Seçin",
    filters: [
      { name: "Metin ve Kod", pattern: "*.txt;*.md;*.json;*.js;*.html;*.css;*.cpp;*.hpp" },
      { name: "Tüm Dosyalar", pattern: "*.*" }
    ]
  });
  if (!path) {
    setOutput("Dosya seçimi iptal edildi.");
    return;
  }
  const info = await rh.fs.stat(path);
  const content = await rh.fs.readText(path);
  const preview = content.length > 400 ? `${content.slice(0, 400)}\n...` : content;
  setOutput(`Dosya: ${path}\nBoyut: ${info.size} bayt\n\nÖnizleme:\n${preview}`);
}

export async function handleListFolder() {
  const folder = await rh.dialog.openFolder({ title: "Listelenecek Klasörü Seçin" });
  if (!folder) {
    setOutput("Klasör seçimi iptal edildi.");
    return;
  }
  setOutput("Klasör okunuyor...");
  const entries = await rh.fs.listDir(folder);
  const dirs = entries.filter((e) => e.isDirectory).length;
  const files = entries.length - dirs;
  const sample = entries
    .slice(0, 8)
    .map((e) => `${e.isDirectory ? "[DIR]" : "[FILE]"} ${e.name}`)
    .join("\n");
  setOutput(`Klasör: ${folder}\nKlasör: ${dirs}, Dosya: ${files}\n\n${sample || "(Boş klasör)"}`);
}

export async function handleSaveReport() {
  const savePath = await rh.dialog.saveFile({
    title: "Raporu Kaydet",
    defaultName: "rhenium-rapor.json",
    filters: [{ name: "JSON Dosyası", pattern: "*.json" }]
  });
  if (!savePath) {
    setOutput("Kaydetme iptal edildi.");
    return;
  }
  const report = {
    generatedAt: new Date().toISOString(),
    operator: state.greetingName,
    counter: state.counter,
    todos: state.todos
  };
  await rh.fs.writeText(savePath, JSON.stringify(report, null, 2));
  setOutput(`Rapor kaydedildi:\n${savePath}`);
}

export async function handleCopySummary() {
  const text = `Rhenium JS+C++ | Sayaç=${state.counter} | ${todoSummaryText()}`;
  const ok = await rh.clipboard.writeText(text);
  setOutput(ok ? `Panoya kopyalandı:\n${text}` : "Panoya yazılamadı.");
}

export async function handleReadClipboard() {
  const text = await rh.clipboard.readText();
  setOutput(text !== null ? `Panodan okunan:\n${text}` : "Panoda metin bulunamadı.");
}

export async function handleMessageBox() {
  const yes = await rh.dialog.messageBox({
    title: "Rhenium Native Onay",
    message: "Bu mesaj kutusu JavaScript (native.js) tarafından window.rhenium.dialog.messageBox ile açıldı.\nDevam edilsin mi?",
    type: "question"
  });
  setOutput(`Mesaj kutusu yanıtı: ${yes ? "Evet" : "Hayır"}`);
}

export async function loadRuntimeInfo() {
  try {
    const info = await rh.app.getInfo();
    $("runtime-mode").textContent = info.embedded ? "Release (Gömülü)" : "Debug (Live-Reload)";
  } catch (_) {}
}
