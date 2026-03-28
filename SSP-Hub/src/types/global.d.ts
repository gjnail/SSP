import type { SspHubApi } from "../../shared/contracts";

declare global {
  interface Window {
    sspHub: SspHubApi;
  }
}

export {};
