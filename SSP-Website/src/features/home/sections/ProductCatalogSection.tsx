import { SectionHeading } from "@/components/ui/SectionHeading";
import { ProductCard } from "@/features/home/components/ProductCard";
import { products } from "@/features/home/content";

export function ProductCatalogSection() {
  return (
    <section className="section" id="products">
      <div className="container">
        <SectionHeading
          label="Releases"
          title="The lineup is presented like a release board, not a stack of polite cards."
          description="Each product is framed as its own utility with a specific reason to exist. The layout is still reusable, but the visual language now feels a lot closer to audio gear and release notes."
        />

        <div className="product-grid">
          {products.map((product, index) => (
            <ProductCard key={product.slug} product={product} index={index} />
          ))}
        </div>
      </div>
    </section>
  );
}
