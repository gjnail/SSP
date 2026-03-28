import { Badge } from "@/components/ui/Badge";
import { Button } from "@/components/ui/Button";
import type { Product } from "@/features/home/content";

type ProductCardProps = {
  product: Product;
  index: number;
};

export function ProductCard({ product, index }: ProductCardProps) {
  return (
    <article className="product-card" data-tone={product.tone}>
      <div className="product-card__rail">
        <span className="product-card__index">{String(index + 1).padStart(2, "0")}</span>
        <Badge className="product-card__label">{product.label}</Badge>
      </div>

      <div className="product-card__body">
        <header className="product-card__header">
          <div>
            <h3>{product.name}</h3>
            <p>{product.strapline}</p>
          </div>
          <span className="product-card__best-for">{product.bestFor}</span>
        </header>

        <p className="product-card__summary">{product.summary}</p>

        <ul className="product-card__list">
          {product.highlights.map((highlight) => (
            <li key={highlight}>{highlight}</li>
          ))}
        </ul>
      </div>

      <div className="product-card__meta">
        <div>
          <span className="product-card__meta-label">Best for</span>
          <strong>{product.bestFor}</strong>
        </div>
        <div>
          <span className="product-card__meta-label">Trigger</span>
          <strong>{product.triggerModes.join(" / ")}</strong>
        </div>
        <div>
          <span className="product-card__meta-label">Formats</span>
          <strong>{product.formats.join(" / ")}</strong>
        </div>
        <div>
          <span className="product-card__meta-label">Platforms</span>
          <strong>{product.platforms.join(" / ")}</strong>
        </div>
        <div className="product-card__actions">
          <Button href="#contact">Request Pricing</Button>
          <Button href="#workflow" variant="ghost">
            See System
          </Button>
        </div>
      </div>
    </article>
  );
}
