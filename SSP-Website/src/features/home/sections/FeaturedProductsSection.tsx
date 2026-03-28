import { Button } from "@/components/ui/Button";
import { SectionHeading } from "@/components/ui/SectionHeading";
import { FeaturedProductCard } from "@/features/home/components/FeaturedProductCard";
import { products } from "@/features/home/content";

export function FeaturedProductsSection() {
  const featured = products.filter((product) => product.status === "available").slice(0, 3);

  return (
    <section className="section">
      <div className="container">
        <SectionHeading
          label="Featured"
          title="A tighter shelf for the releases most likely to define the SSP lineup first."
          description="The front page stays focused by surfacing a few flagship tools, then handing off to the full catalog once visitors want to browse deeper."
        />

        <div className="featured-products__grid">
          {featured.map((product) => (
            <FeaturedProductCard key={product.slug} product={product} />
          ))}
        </div>

        <div className="featured-products__actions">
          <Button to="/products">See All Plugins</Button>
          <Button to="/hub" variant="ghost">
            See SSP Hub
          </Button>
        </div>
      </div>
    </section>
  );
}
