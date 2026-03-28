import type {
  AnchorHTMLAttributes,
  ButtonHTMLAttributes,
  PropsWithChildren,
} from "react";
import { Link, type LinkProps } from "react-router-dom";
import { cn } from "@/lib/cn";

type ButtonVariant = "primary" | "secondary" | "ghost";

type SharedProps = PropsWithChildren<{
  className?: string;
  variant?: ButtonVariant;
}>;

type ButtonAsAnchor = SharedProps &
  AnchorHTMLAttributes<HTMLAnchorElement> & {
    href: string;
    to?: never;
  };

type ButtonAsNative = SharedProps &
  ButtonHTMLAttributes<HTMLButtonElement> & {
    href?: never;
    to?: never;
  };

type ButtonAsLink = SharedProps &
  LinkProps & {
    to: string;
    href?: never;
  };

function getButtonClassName(variant: ButtonVariant, className?: string) {
  return cn("button", `button--${variant}`, className);
}

export function Button(props: ButtonAsAnchor | ButtonAsNative | ButtonAsLink) {
  const variant = props.variant ?? "primary";

  if ("to" in props && typeof props.to === "string") {
    const { children, className, to, variant: _variant, ...rest } = props;
    const linkProps = rest as Omit<LinkProps, "to">;

    return (
      <Link className={getButtonClassName(variant, className)} to={to} {...linkProps}>
        {children}
      </Link>
    );
  }

  if ("href" in props && props.href) {
    const { children, className, href, variant: _variant, ...rest } = props;
    const anchorProps = rest as AnchorHTMLAttributes<HTMLAnchorElement>;

    return (
      <a className={getButtonClassName(variant, className)} href={href} {...anchorProps}>
        {children}
      </a>
    );
  }

  const { children, className, type, variant: _variant, ...rest } = props;
  const buttonProps = rest as ButtonHTMLAttributes<HTMLButtonElement>;

  const nativeType: "button" | "submit" | "reset" =
    type === "submit" || type === "reset" ? type : "button";

  return (
    <button className={getButtonClassName(variant, className)} type={nativeType} {...buttonProps}>
      {children}
    </button>
  );
}
