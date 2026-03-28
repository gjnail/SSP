import { createContext, useContext, useEffect, useState, type ReactNode } from "react";

import type { HubSnapshot, SaveSettingsInput } from "../../shared/contracts";

interface HubContextValue {
  activateProduct: (productId: string) => Promise<void>;
  browseForFolder: (initialPath?: string) => Promise<string | null>;
  buildAllWindowsInstallers: () => Promise<void>;
  buildInstaller: (productId: string) => Promise<void>;
  busyAction: string | null;
  errorMessage: string | null;
  deactivateProduct: (productId: string) => Promise<void>;
  installProduct: (productId: string) => Promise<void>;
  loading: boolean;
  refresh: () => Promise<void>;
  revealArtifact: (productId: string) => Promise<void>;
  saveSettings: (input: SaveSettingsInput) => Promise<void>;
  signIn: (email: string) => Promise<void>;
  snapshot: HubSnapshot | null;
  uninstallProduct: (productId: string) => Promise<void>;
  updateProduct: (productId: string) => Promise<void>;
}

const HubContext = createContext<HubContextValue | null>(null);

function getErrorMessage(error: unknown) {
  if (error instanceof Error) {
    return error.message;
  }

  return "Something went wrong inside SSP Hub.";
}

export function HubProvider({ children }: { children: ReactNode }) {
  const [snapshot, setSnapshot] = useState<HubSnapshot | null>(null);
  const [loading, setLoading] = useState(true);
  const [errorMessage, setErrorMessage] = useState<string | null>(null);
  const [busyAction, setBusyAction] = useState<string | null>(null);

  async function loadSnapshot() {
    setLoading(true);

    try {
      const nextSnapshot = await window.sspHub.getSnapshot();
      setSnapshot(nextSnapshot);
      setErrorMessage(null);
    } catch (error) {
      setErrorMessage(getErrorMessage(error));
    } finally {
      setLoading(false);
    }
  }

  useEffect(() => {
    void loadSnapshot();
  }, []);

  useEffect(() => {
    if (!snapshot) {
      return;
    }

    document.body.dataset.theme = snapshot.settings.themeMode;
  }, [snapshot]);

  async function runAction(actionKey: string, runner: () => Promise<HubSnapshot>) {
    setBusyAction(actionKey);

    try {
      const nextSnapshot = await runner();
      setSnapshot(nextSnapshot);
      setErrorMessage(null);
    } catch (error) {
      setErrorMessage(getErrorMessage(error));
    } finally {
      setBusyAction(null);
    }
  }

  return (
    <HubContext.Provider
      value={{
        activateProduct: (productId: string) =>
          runAction(`activate:${productId}`, () => window.sspHub.activateProduct(productId)),
        browseForFolder: (initialPath?: string) => window.sspHub.browseForFolder(initialPath),
        buildAllWindowsInstallers: () =>
          runAction("build-all-installers", () => window.sspHub.buildAllWindowsInstallers()),
        buildInstaller: (productId: string) =>
          runAction(`build:${productId}`, () => window.sspHub.buildInstaller(productId)),
        deactivateProduct: (productId: string) =>
          runAction(`deactivate:${productId}`, () => window.sspHub.deactivateProduct(productId)),
        busyAction,
        errorMessage,
        installProduct: (productId: string) =>
          runAction(`install:${productId}`, () => window.sspHub.installProduct(productId)),
        loading,
        refresh: loadSnapshot,
        revealArtifact: async (productId: string) => {
          setBusyAction(`reveal:${productId}`);

          try {
            await window.sspHub.revealArtifact(productId);
          } catch (error) {
            setErrorMessage(getErrorMessage(error));
          } finally {
            setBusyAction(null);
          }
        },
        saveSettings: (input: SaveSettingsInput) =>
          runAction("save-settings", () => window.sspHub.saveSettings(input)),
        signIn: (email: string) => runAction("sign-in", () => window.sspHub.signIn(email)),
        snapshot,
        uninstallProduct: (productId: string) =>
          runAction(`uninstall:${productId}`, () => window.sspHub.uninstallProduct(productId)),
        updateProduct: (productId: string) =>
          runAction(`update:${productId}`, () => window.sspHub.updateProduct(productId)),
      }}
    >
      {children}
    </HubContext.Provider>
  );
}

export function useHub() {
  const context = useContext(HubContext);

  if (!context) {
    throw new Error("useHub must be used inside HubProvider.");
  }

  return context;
}
