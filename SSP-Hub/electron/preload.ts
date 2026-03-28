import { contextBridge, ipcRenderer } from "electron";

import type { SaveSettingsInput, SspHubApi } from "../shared/contracts";

const api: SspHubApi = {
  activateProduct: (productId: string) => ipcRenderer.invoke("hub:activate-product", productId),
  browseForFolder: (initialPath?: string) => ipcRenderer.invoke("hub:browse-for-folder", initialPath),
  buildAllWindowsInstallers: () => ipcRenderer.invoke("hub:build-all-windows-installers"),
  buildInstaller: (productId: string) => ipcRenderer.invoke("hub:build-installer", productId),
  deactivateProduct: (productId: string) => ipcRenderer.invoke("hub:deactivate-product", productId),
  getSnapshot: () => ipcRenderer.invoke("hub:get-snapshot"),
  installProduct: (productId: string) => ipcRenderer.invoke("hub:install-product", productId),
  revealArtifact: (productId: string) => ipcRenderer.invoke("hub:reveal-artifact", productId),
  saveSettings: (input: SaveSettingsInput) => ipcRenderer.invoke("hub:save-settings", input),
  signIn: (email: string) => ipcRenderer.invoke("hub:sign-in", email),
  uninstallProduct: (productId: string) => ipcRenderer.invoke("hub:uninstall-product", productId),
  updateProduct: (productId: string) => ipcRenderer.invoke("hub:update-product", productId),
};

contextBridge.exposeInMainWorld("sspHub", api);
