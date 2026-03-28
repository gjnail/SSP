import { Button } from "@/components/ui/Button";

export function NotFoundPage() {
  return (
    <section className="section page-shell">
      <div className="container page-stack">
        <div className="page-section-heading">
          <p className="page-eyebrow">404</p>
          <h1>That page is not in the storefront.</h1>
          <p>Head back to the home page or jump straight into the plugin catalog.</p>
          <div className="featured-products__actions">
            <Button to="/">Go Home</Button>
            <Button to="/products" variant="ghost">
              Browse Plugins
            </Button>
          </div>
        </div>
      </div>
    </section>
  );
}
