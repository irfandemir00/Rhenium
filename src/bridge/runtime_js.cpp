#include "rhenium/bridge/runtime_js.hpp"

namespace rhenium {

const std::string& get_runtime_script() {
    static const std::string kScript = R"RHENIUM_JS(
(function () {
    if (window.__RHENIUM_INITIALIZED__) return;
    window.__RHENIUM_INITIALIZED__ = true;

    const webview = window.chrome && window.chrome.webview;
    if (!webview) return;

    let nextReqId = 1;
    const pending = new Map();
    const eventListeners = new Map();

    function post(payload) {
        webview.postMessage(payload);
    }

    function sendInternal(cmd) {
        post({ kind: "internal", cmd: cmd });
    }

    function invoke(cmd, args) {
        return new Promise((resolve, reject) => {
            const id = nextReqId++;
            pending.set(id, { resolve, reject });
            post({
                kind: "invoke",
                id: id,
                cmd: cmd,
                args: args === undefined ? [] : args
            });
        });
    }

    function reloadStylesheets() {
        const links = document.querySelectorAll('link[rel="stylesheet"]');
        const stamp = Date.now();
        links.forEach((link) => {
            const rawHref = link.getAttribute("href");
            if (!rawHref) return;
            const base = rawHref.split("?")[0];
            link.setAttribute("href", base + "?_rh=" + stamp);
        });
    }

    webview.addEventListener("message", (event) => {
        const msg = event.data;
        if (!msg || typeof msg !== "object") return;

        if (msg.kind === "response") {
            const waiter = pending.get(msg.id);
            if (!waiter) return;
            pending.delete(msg.id);
            if (msg.ok) waiter.resolve(msg.value);
            else waiter.reject(new Error(msg.error || "Rhenium IPC error"));
            return;
        }

        if (msg.kind === "event") {
            const set = eventListeners.get(msg.name);
            if (set) {
                set.forEach((fn) => { try { fn(msg.payload); } catch (_) {} });
            }
            return;
        }

        if (msg.kind === "reload_css") {
            reloadStylesheets();
            return;
        }

        if (msg.kind === "reload_page") {
            window.location.reload();
        }
    });

    function setupDragRegion() {
        const INTERACTIVE = "button, a, input, select, textarea, label, [data-rh-no-drag]";

        document.addEventListener("mousedown", (e) => {
            if (e.button !== 0 || !(e.target instanceof Element)) return;
            const drag = e.target.closest("[data-rh-drag-region]");
            if (!drag || e.target.closest(INTERACTIVE)) return;
            e.preventDefault();
            sendInternal("__rh_window_start_drag");
        });

        document.addEventListener("dblclick", (e) => {
            if (!(e.target instanceof Element)) return;
            const drag = e.target.closest("[data-rh-drag-region]");
            if (!drag || e.target.closest(INTERACTIVE)) return;
            sendInternal("__rh_window_toggle_maximize");
        });
    }

    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", setupDragRegion);
    } else {
        setupDragRegion();
    }

    window.rhenium = {
        invoke: invoke,
        on: function (eventName, fn) {
            if (!eventListeners.has(eventName)) eventListeners.set(eventName, new Set());
            eventListeners.get(eventName).add(fn);
            return function () {
                const set = eventListeners.get(eventName);
                if (set) set.delete(fn);
            };
        },
        window: {
            minimize: function () { return invoke("rh:window:minimize"); },
            toggleMaximize: function () { return invoke("rh:window:toggleMaximize"); },
            isMaximized: function () { return invoke("rh:window:isMaximized"); },
            close: function () { return invoke("rh:window:close"); },
            setTitle: function (title) { return invoke("rh:window:setTitle", [String(title ?? "")]); },
            startDrag: function () { return invoke("rh:window:startDrag"); },
            openDevTools: function () { return invoke("rh:window:openDevTools"); },
            reload: function () { return invoke("rh:window:reload"); }
        },
        dialog: {
            openFile: function (options) { return invoke("rh:dialog:openFile", [options || {}]); },
            saveFile: function (options) { return invoke("rh:dialog:saveFile", [options || {}]); },
            openFolder: function (options) { return invoke("rh:dialog:openFolder", [options || {}]); },
            messageBox: function (options) { return invoke("rh:dialog:messageBox", [options || {}]); }
        },
        clipboard: {
            readText: function () { return invoke("rh:clipboard:readText"); },
            writeText: function (text) { return invoke("rh:clipboard:writeText", [String(text ?? "")]); }
        },
        fs: {
            readText: function (path) { return invoke("rh:fs:readText", [String(path)]); },
            writeText: function (path, content) { return invoke("rh:fs:writeText", [String(path), String(content ?? "")]); },
            exists: function (path) { return invoke("rh:fs:exists", [String(path)]); },
            stat: function (path) { return invoke("rh:fs:stat", [String(path)]); },
            listDir: function (path) { return invoke("rh:fs:listDir", [String(path)]); }
        },
        shell: {
            openExternal: function (url) { return invoke("rh:shell:openExternal", [String(url)]); }
        },
        app: {
            getInfo: function () { return invoke("rh:app:getInfo"); }
        }
    };
})();
)RHENIUM_JS";
    return kScript;
}

}
