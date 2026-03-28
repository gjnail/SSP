import { SectionHeading } from "@/components/ui/SectionHeading";
import { principles } from "@/features/home/content";

export function ManifestoSection() {
  return (
    <section className="section section--muted">
      <div className="container">
        <SectionHeading
          label="Why SSP"
          title="A more traditional plugin storefront, without throwing away the current style."
          description="This section now sells the value of the brand more directly: focused tools, clearer product framing, and a storefront structure that feels familiar to plugin buyers."
        />

        <div className="principle-grid">
          {principles.map((principle) => (
            <article key={principle.title} className="principle-card">
              <h3>{principle.title}</h3>
              <p>{principle.body}</p>
            </article>
          ))}
        </div>
      </div>
    </section>
  );
}
