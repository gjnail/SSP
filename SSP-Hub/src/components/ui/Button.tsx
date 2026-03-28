import type { ButtonHTMLAttributes, PropsWithChildren } from "react";

type ButtonTone = "accent" | "danger" | "ghost" | "secondary";

interface ButtonProps extends ButtonHTMLAttributes<HTMLButtonElement> {
  tone?: ButtonTone;
}

export function Button({
  children,
  className = "",
  tone = "accent",
  type = "button",
  ...props
}: PropsWithChildren<ButtonProps>) {
  return (
    <button className={`button button--${tone} ${className}`.trim()} type={type} {...props}>
      {children}
    </button>
  );
}
