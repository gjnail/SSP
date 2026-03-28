import { useEffect, useState } from "react";

import { useHub } from "../../app/HubProvider";
import { Badge } from "../../components/ui/Badge";
import { Button } from "../../components/ui/Button";
import { Panel } from "../../components/ui/Panel";

const DEMO_ACCOUNTS = [
  "studio@stupidsimpleplugins.com",
  "mix@stupidsimpleplugins.com",
  "starter@stupidsimpleplugins.com",
];

export function SettingsPage() {
  const { browseForFolder, busyAction, saveSettings, signIn, snapshot } = useHub();
  const [email, setEmail] = useState("");
  const [licensesPath, setLicensesPath] = useState("");
  const [windowsPath, setWindowsPath] = useState("");
  const [macPath, setMacPath] = useState("");

  useEffect(() => {
    if (!snapshot) {
      return;
    }

    setEmail(snapshot.account.email);
    setLicensesPath(snapshot.settings.licensesPath);
    setWindowsPath(snapshot.settings.customVst3Paths.windows);
    setMacPath(snapshot.settings.customVst3Paths.macos);
  }, [snapshot]);

  if (!snapshot) {
    return null;
  }

  async function chooseFolder(target: "windows" | "macos") {
    const currentPath = target === "windows" ? windowsPath : macPath;
    const nextPath = await browseForFolder(currentPath);

    if (!nextPath) {
      return;
    }

    if (target === "windows") {
      setWindowsPath(nextPath);
    } else {
      setMacPath(nextPath);
    }
  }

  return (
    <div className="page page--stack">
      <div className="section-heading">
        <div>
          <span className="section-heading__eyebrow">Settings</span>
          <h1>Account, install paths, and staging preferences.</h1>
        </div>
      </div>

      <div className="settings-grid">
        <Panel>
          <h2>Account</h2>
          <p className="muted-copy">
            This prototype uses local demo entitlements. Change the email to preview different owned-plugin states.
          </p>
          <form
            className="settings-form"
            onSubmit={(event) => {
              event.preventDefault();
              void signIn(email);
            }}
          >
            <label className="field">
              <span>Email</span>
              <input onChange={(event) => setEmail(event.target.value)} type="email" value={email} />
            </label>
            <Button disabled={busyAction === "sign-in"} type="submit">
              {busyAction === "sign-in" ? "Switching..." : "Sign in"}
            </Button>
          </form>

          <div className="demo-accounts">
            {DEMO_ACCOUNTS.map((demoAccount) => (
              <button
                key={demoAccount}
                className="demo-account"
                onClick={() => {
                  setEmail(demoAccount);
                  void signIn(demoAccount);
                }}
                type="button"
              >
                {demoAccount}
              </button>
            ))}
          </div>
        </Panel>

        <Panel>
          <h2>Install destinations</h2>
          <p className="muted-copy">
            Set the VST3 destination for each platform, choose where SSP Hub should cache local license files, and control the app theme.
          </p>
          <form
            className="settings-form"
            onSubmit={(event) => {
              event.preventDefault();
              void saveSettings({
                customVst3Paths: {
                  macos: macPath,
                  windows: windowsPath,
                },
                licensesPath,
              });
            }}
          >
            <label className="field">
              <span>Windows VST3 path</span>
              <div className="field__row">
                <input onChange={(event) => setWindowsPath(event.target.value)} value={windowsPath} />
                <Button onClick={() => void chooseFolder("windows")} tone="ghost">
                  Browse
                </Button>
              </div>
            </label>
            <label className="field">
              <span>macOS VST3 path</span>
              <div className="field__row">
                <input onChange={(event) => setMacPath(event.target.value)} value={macPath} />
                <Button onClick={() => void chooseFolder("macos")} tone="ghost">
                  Browse
                </Button>
              </div>
            </label>
            <label className="field">
              <span>Licenses folder</span>
              <div className="field__row">
                <input onChange={(event) => setLicensesPath(event.target.value)} value={licensesPath} />
                <Button onClick={() => browseForFolder(licensesPath).then((nextPath) => nextPath && setLicensesPath(nextPath))} tone="ghost">
                  Browse
                </Button>
              </div>
            </label>

            <div className="settings-inline">
              <Badge tone="muted">Workspace root</Badge>
              <span>{snapshot.settings.workspaceRoot}</span>
            </div>
            <div className="settings-inline">
              <Badge tone="muted">Theme</Badge>
              <span>{snapshot.settings.themeMode === "dark" ? "Dark mode enabled" : "Light mode enabled"}</span>
            </div>

            <Button disabled={busyAction === "save-settings"} type="submit">
              {busyAction === "save-settings" ? "Saving..." : "Save settings"}
            </Button>
          </form>
        </Panel>
      </div>
    </div>
  );
}
