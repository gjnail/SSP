import fs from "node:fs";
import path from "node:path";

import { app, BrowserWindow, dialog, ipcMain } from "electron";

import {
  activateProduct,
  buildAllWindowsInstallers,
  buildInstaller,
  deactivateProduct,
  getSnapshot,
  installProduct,
  revealArtifact,
  saveSettings,
  signIn,
  uninstallProduct,
  updateProduct,
} from "./services/hubService";

const isDevelopment = Boolean(process.env.ELECTRON_RENDERER_URL);
const userDataPath = path.join(app.getPath("appData"), "SSP Hub");
const sessionDataPath = path.join(userDataPath, "SessionData");

app.setName("SSP Hub");
app.setPath("userData", userDataPath);
app.setPath("sessionData", sessionDataPath);

fs.mkdirSync(userDataPath, { recursive: true });
fs.mkdirSync(sessionDataPath, { recursive: true });

function createWindow() {
  const mainWindow = new BrowserWindow({
    width: 1480,
    height: 920,
    minWidth: 1180,
    minHeight: 760,
    backgroundColor: "#0b1118",
    title: "SSP Hub",
    webPreferences: {
      contextIsolation: true,
      preload: path.join(__dirname, "preload.js"),
    },
  });

  if (isDevelopment) {
    void mainWindow.loadURL(process.env.ELECTRON_RENDERER_URL!);
  } else {
    void mainWindow.loadFile(path.join(__dirname, "..", "..", "dist", "index.html"));
  }
}

function registerIpc() {
  ipcMain.handle("hub:get-snapshot", () => getSnapshot());
  ipcMain.handle("hub:activate-product", (_event, productId: string) => activateProduct(productId));
  ipcMain.handle("hub:build-all-windows-installers", () => buildAllWindowsInstallers());
  ipcMain.handle("hub:build-installer", (_event, productId: string) => buildInstaller(productId));
  ipcMain.handle("hub:deactivate-product", (_event, productId: string) => deactivateProduct(productId));
  ipcMain.handle("hub:sign-in", (_event, email: string) => signIn(email));
  ipcMain.handle("hub:save-settings", (_event, input) => saveSettings(input));
  ipcMain.handle("hub:install-product", (_event, productId: string) => installProduct(productId));
  ipcMain.handle("hub:update-product", (_event, productId: string) => updateProduct(productId));
  ipcMain.handle("hub:uninstall-product", (_event, productId: string) => uninstallProduct(productId));
  ipcMain.handle("hub:reveal-artifact", (_event, productId: string) => revealArtifact(productId));
  ipcMain.handle("hub:browse-for-folder", async (_event, initialPath?: string) => {
    const result = await dialog.showOpenDialog({
      defaultPath: initialPath,
      properties: ["openDirectory", "createDirectory"],
      title: "Choose a plugin install folder",
    });

    return result.canceled ? null : result.filePaths[0] ?? null;
  });
}

app.whenReady().then(() => {
  registerIpc();
  createWindow();

  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

app.on("window-all-closed", () => {
  if (process.platform !== "darwin") {
    app.quit();
  }
});
