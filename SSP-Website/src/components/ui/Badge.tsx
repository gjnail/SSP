import type { PropsWithChildren } from "react";
import { cn } from "@/lib/cn";

type BadgeProps = PropsWithChildren<{
  className?: string;
}>;

export function Badge({ children, className }: BadgeProps) {
  return <span className={cn("badge", className)}>{children}</span>;
}
