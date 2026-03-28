import { SectionHeading } from "@/components/ui/SectionHeading";
import { workflowSteps } from "@/features/home/content";

export function WorkflowSection() {
  return (
    <section className="section" id="workflow">
      <div className="container workflow-grid">
        <div className="workflow-copy">
          <SectionHeading
            label="Which One?"
            title="Help visitors choose the right lane first."
            description="The SSP lineup is larger now, so the site needs to guide people toward the right category quickly without flattening everything into the same sales pitch."
          />

          <div className="workflow-benefits">
            <article className="workflow-benefit">
              <span>Included</span>
              <h3>VST3 and standalone clarity</h3>
              <p>Each active plugin is framed with the formats and platform positioning already represented in the actual SSP workspace.</p>
            </article>
            <article className="workflow-benefit">
              <span>Hub</span>
              <h3>Desktop install path in the story</h3>
              <p>SSP Hub now has dedicated pages so the site can explain how downloads, installs, updates, and activation are meant to connect later.</p>
            </article>
            <article className="workflow-benefit">
              <span>Lineup</span>
              <h3>Focused catalog instead of plugin bloat</h3>
              <p>Each release still has a narrow reason to exist, which helps the larger lineup stay readable even as more categories get added.</p>
            </article>
          </div>
        </div>

        <div className="workflow-steps">
          {workflowSteps.map((item) => (
            <article key={item.step} className="workflow-card">
              <span className="workflow-card__step">{item.step}</span>
              <h3>{item.title}</h3>
              <p>{item.body}</p>
            </article>
          ))}
        </div>
      </div>
    </section>
  );
}
