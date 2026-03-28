import { Badge } from "@/components/ui/Badge";
import { cn } from "@/lib/cn";

type SectionHeadingProps = {
  label: string;
  title: string;
  description: string;
  className?: string;
};

export function SectionHeading({ label, title, description, className }: SectionHeadingProps) {
  return (
    <div className={cn("section-heading", className)}>
      <Badge>{label}</Badge>
      <h2>{title}</h2>
      <p>{description}</p>
    </div>
  );
}
