import { Outlet } from "react-router-dom";
import { SiteFooter } from "@/components/layout/SiteFooter";
import { SiteHeader } from "@/components/layout/SiteHeader";

export function SiteLayout() {
  return (
    <div className="site-shell">
      <SiteHeader />
      <main>
        <Outlet />
      </main>
      <SiteFooter />
    </div>
  );
}
