import type { PropsWithChildren } from "react";

type BadgeTone = "accent" | "muted" | "success" | "warning";

export function Badge({
  children,
  tone = "muted",
}: PropsWithChildren<{ tone?: BadgeTone }>) {
  return <span className={`badge badge--${tone}`}>{children}</span>;
}
