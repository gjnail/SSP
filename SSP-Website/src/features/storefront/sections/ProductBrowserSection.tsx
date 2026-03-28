import { useDeferredValue, useState } from "react";
import { SectionHeading } from "@/components/ui/SectionHeading";
import {
  products,
  type Product,
  type ProductCategory,
} from "@/features/home/content";
import { CartPanel } from "@/features/storefront/components/CartPanel";
import { StoreProductCard } from "@/features/storefront/components/StoreProductCard";

type SortMode = "featured" | "price-low" | "price-high" | "name";
type CategoryFilter = ProductCategory | "all";

const categoryFilters: Array<{ label: string; value: CategoryFilter }> = [
  { label: "All", value: "all" },
  { label: "Sidechain", value: "sidechain" },
  { label: "Dynamics", value: "dynamics" },
  { label: "Tone", value: "tone" },
  { label: "Space", value: "space" },
  { label: "Analysis", value: "analysis" },
  { label: "Creative", value: "creative" },
  { label: "Instrument", value: "instrument" },
];

function sortProducts(entries: Product[], sortMode: SortMode) {
  const next = [...entries];

  next.sort((left, right) => {
    if (sortMode === "name") {
      return left.name.localeCompare(right.name);
    }

    if (sortMode === "price-low") {
      return left.priceUsd - right.priceUsd;
    }

    if (sortMode === "price-high") {
      return right.priceUsd - left.priceUsd;
    }

    if (left.status !== right.status) {
      return left.status === "available" ? -1 : 1;
    }

    return products.findIndex((entry) => entry.slug === left.slug) -
      products.findIndex((entry) => entry.slug === right.slug);
  });

  return next;
}

export function ProductBrowserSection() {
  const [query, setQuery] = useState("");
  const [category, setCategory] = useState<CategoryFilter>("all");
  const [sortMode, setSortMode] = useState<SortMode>("featured");
  const deferredQuery = useDeferredValue(query);
  const normalizedQuery = deferredQuery.trim().toLowerCase();

  const filteredProducts = sortProducts(
    products.filter((product) => {
      const matchesCategory = category === "all" || product.category === category;
      const haystack = [
        product.name,
        product.label,
        product.strapline,
        product.summary,
        product.bestFor,
        ...product.highlights,
        ...product.triggerModes,
      ]
        .join(" ")
        .toLowerCase();
      const matchesQuery = normalizedQuery.length === 0 || haystack.includes(normalizedQuery);

      return matchesCategory && matchesQuery;
    }),
    sortMode,
  );

  return (
    <section className="section" id="products">
      <div className="container">
        <SectionHeading
          label="Plugins"
          title="Choose the plugin that fits the job, then add it to cart."
          description="The catalog now reflects the current SSP lineup across sidechain, tone, dynamics, analysis, creative FX, and instrument slots, with the cart still parked right beside it."
        />

        <div className="browser-toolbar">
          <label className="browser-toolbar__search">
            Search releases
            <input
              onChange={(event) => setQuery(event.target.value)}
              placeholder="Search sidechain, saturate, loudness, delay..."
              type="search"
              value={query}
            />
          </label>

          <div className="browser-toolbar__filters" role="tablist" aria-label="Category filters">
            {categoryFilters.map((filter) => (
              <button
                key={filter.value}
                className={filter.value === category ? "is-active" : undefined}
                onClick={() => setCategory(filter.value)}
                type="button"
              >
                {filter.label}
              </button>
            ))}
          </div>

          <label className="browser-toolbar__sort">
            Sort
            <select
              onChange={(event) => setSortMode(event.target.value as SortMode)}
              value={sortMode}
            >
              <option value="featured">Featured</option>
              <option value="name">Name</option>
              <option value="price-low">Price low to high</option>
              <option value="price-high">Price high to low</option>
            </select>
          </label>
        </div>

        <div className="browser-layout">
          <div className="browser-results">
            <div className="browser-results__meta">
              <span>{filteredProducts.length} products shown</span>
              <span>
                {products.filter((product) => product.status === "available").length} available /{" "}
                {products.filter((product) => product.status === "coming-soon").length} coming soon
              </span>
            </div>

            <div className="browser-results__grid">
              {filteredProducts.map((product) => (
                <StoreProductCard key={product.slug} product={product} />
              ))}
            </div>
          </div>

          <CartPanel />
        </div>
      </div>
    </section>
  );
}
