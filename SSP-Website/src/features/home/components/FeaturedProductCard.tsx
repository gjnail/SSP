import { Link } from "react-router-dom";
import type { Product } from "@/features/home/content";

type FeaturedProductCardProps = {
  product: Product;
};

export function FeaturedProductCard({ product }: FeaturedProductCardProps) {
  return (
    <article className="featured-product" data-tone={product.tone}>
      <img alt={`${product.name} temporary product artwork`} src={product.image} />
      <div className="featured-product__body">
        <span className="featured-product__eyebrow">{product.label}</span>
        <h3>{product.name}</h3>
        <p>{product.summary}</p>
        <div className="featured-product__meta">
          <strong>{product.status === "available" ? `$${product.priceUsd}` : "Coming Soon"}</strong>
          <span>{product.bestFor}</span>
        </div>
        <Link className="button button--ghost" to={`/products/${product.slug}`}>
          View Product
        </Link>
      </div>
    </article>
  );
}
