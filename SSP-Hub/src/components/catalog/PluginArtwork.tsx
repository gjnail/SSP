function buildProductCode(title: string, productId: string) {
  const cleanTitle = title.replace(/^SSP\s*/i, "").trim().toUpperCase();
  const words = cleanTitle.split(/\s+/).filter(Boolean);
  const initials = words.map((word) => word[0]).join("");
  const joined = words.join("");
  const consonants = joined.replace(/[AEIOU]/g, "");
  const digits = productId.replace(/\D/g, "");

  if (words.length >= 2) {
    return `${initials}${consonants.slice(0, Math.max(0, 4 - initials.length))}`.slice(0, 4);
  }

  return `${joined.slice(0, 4)}${digits}`.slice(0, 5);
}

function getArtworkVariant(productId: string) {
  const variants = ["bars", "rings", "diagonal", "stack"] as const;
  const total = productId.split("").reduce((sum, character) => sum + character.charCodeAt(0), 0);
  return variants[total % variants.length];
}

export function PluginArtwork({ productId, title }: { productId: string; title: string }) {
  const seed = buildProductCode(title, productId);
  const variant = getArtworkVariant(productId);

  return (
    <div className={`plugin-art plugin-art--${variant}`}>
      <div className="plugin-art__grid" />
      <div className="plugin-art__motif" />
      <span className="plugin-art__label">SSP</span>
      <strong className="plugin-art__seed">{seed}</strong>
      <span className="plugin-art__title">{title}</span>
    </div>
  );
}
