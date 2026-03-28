import { SectionHeading } from "@/components/ui/SectionHeading";
import { faqItems } from "@/features/home/content";

export function FaqSection() {
  return (
    <section className="section section--faq" id="faq">
      <div className="container">
        <SectionHeading
          label="FAQ"
          title="The practical questions stay, but the presentation is less polite."
          description="This keeps the launch logistics clear while matching the new stronger visual direction."
        />

        <div className="faq-list">
          {faqItems.map((item) => (
            <details key={item.question} className="faq-item">
              <summary>{item.question}</summary>
              <p>{item.answer}</p>
            </details>
          ))}
        </div>
      </div>
    </section>
  );
}
