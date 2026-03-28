import { useState } from "react";
import { Button } from "@/components/ui/Button";

export function ContactPage() {
  const [submitted, setSubmitted] = useState(false);

  return (
    <section className="section page-shell">
      <div className="container contact-grid">
        <div className="page-section-heading">
          <p className="page-eyebrow">Contact</p>
          <h1>Reach out about releases, bundles, beta access, or launch setup.</h1>
          <p>
            This is a placeholder contact flow for now, but it gives the site the dedicated company
            page structure a normal plugin storefront would have.
          </p>
        </div>

        <form
          className="contact-form"
          onSubmit={(event) => {
            event.preventDefault();
            setSubmitted(true);
            event.currentTarget.reset();
          }}
        >
          <label>
            Name
            <input name="name" placeholder="Your name" required />
          </label>
          <label>
            Email
            <input name="email" placeholder="you@example.com" required type="email" />
          </label>
          <label>
            Topic
            <select defaultValue="sales" name="topic">
              <option value="sales">Sales question</option>
              <option value="beta">Beta access</option>
              <option value="press">Press / partnership</option>
              <option value="support">Support</option>
            </select>
          </label>
          <label>
            Message
            <textarea name="message" placeholder="Tell us what you need" required rows={6} />
          </label>
          <Button type="submit">Send Message</Button>
          {submitted ? (
            <p className="contact-form__success">Message captured in the dummy contact flow.</p>
          ) : null}
        </form>
      </div>
    </section>
  );
}
