import { state, todoSummaryText } from "./state.js";
import { $, setOutput } from "./ui.js";

const rh = window.rhenium;

function reportError(err) {
  const msg = err && err.message ? err.message : String(err);
  setOutput(`Hata: ${msg}`);
}

export function wireWindowControls() {
  $("btn-devtools").addEventListener("click", () => rh.window.openDevTools());
  $("btn-minimize").addEventListener("click", () => rh.window.minimize());
  $("btn-maximize").addEventListener("click", () => rh.window.toggleMaximize());
  $("btn-close").addEventListener("click", () => rh.window.close());

  const updateMaxBtn = (maximized) => {
    const btn = $("btn-maximize");
    if (!btn) return;
    btn.textContent = maximized ? "\u2750" : "\u25A1";
    btn.title = maximized ? "Geri Yükle" : "Büyüt";
  };

  rh.on("rhenium:maximized", updateMaxBtn);
  rh.window.isMaximized().then(updateMaxBtn).catch(() => {});
}

export async function handleOpenFile() {
  try {
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
  } catch (err) {
    reportError(err);
  }
}

export async function handleListFolder() {
  try {
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
  } catch (err) {
    reportError(err);
  }
}

export async function handleSaveReport() {
  try {
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
  } catch (err) {
    reportError(err);
  }
}

export async function handleCopySummary() {
  try {
    const text = `Rhenium JS+C++ | Sayaç=${state.counter} | ${todoSummaryText()}`;
    const ok = await rh.clipboard.writeText(text);
    setOutput(ok ? `Panoya kopyalandı:\n${text}` : "Panoya yazılamadı.");
  } catch (err) {
    reportError(err);
  }
}

export async function handleReadClipboard() {
  try {
    const text = await rh.clipboard.readText();
    setOutput(text !== null ? `Panodan okunan:\n${text}` : "Panoda metin bulunamadı.");
  } catch (err) {
    reportError(err);
  }
}

export async function handleMessageBox() {
  try {
    const yes = await rh.dialog.messageBox({
      title: "Rhenium Native Onay",
      message: "Bu mesaj kutusu JavaScript (native.js) tarafından window.rhenium.dialog.messageBox ile açıldı.\nDevam edilsin mi?",
      type: "question"
    });
    setOutput(`Mesaj kutusu yanıtı: ${yes ? "Evet" : "Hayır"}`);
  } catch (err) {
    reportError(err);
  }
}

export async function loadRuntimeInfo() {
  try {
    const info = await rh.app.getInfo();
    $("runtime-mode").textContent = info.embedded ? "Release (Gömülü)" : "Debug (Live-Reload)";
    const devtoolsBtn = $("btn-devtools");
    if (devtoolsBtn) {
      devtoolsBtn.hidden = !info.devtools;
    }
  } catch (_) {}
}
