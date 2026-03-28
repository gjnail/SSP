import type { AccountProfile, CatalogProduct } from "../../shared/contracts";

function allProductIds(catalog: CatalogProduct[]) {
  return catalog.map((product) => product.id);
}

const STARTER_BUNDLE = [
  "simple-sidechain",
  "ssp-clipper",
  "ssp-saturate",
  "ssp-transition",
  "ssp-3osc",
];

const MIX_BUNDLE = [
  "simple-sidechain",
  "ssp-multichain",
  "ssp-eq",
  "ssp-reverb",
  "ssp-delay",
  "ssp-echo",
  "ssp-saturate",
  "ssp-transition",
];

export function resolveAccount(email: string, catalog: CatalogProduct[]): AccountProfile {
  const normalizedEmail = email.trim().toLowerCase() || "studio@stupidsimpleplugins.com";
  const availableIds = new Set(allProductIds(catalog));
  const allowed = (productIds: string[]) => productIds.filter((productId) => availableIds.has(productId));

  if (normalizedEmail === "studio@stupidsimpleplugins.com" || normalizedEmail.includes("all")) {
    return {
      activationLimitPerProduct: 2,
      email: normalizedEmail,
      entitledProductIds: allProductIds(catalog),
      planLabel: "Studio All Access",
    };
  }

  if (normalizedEmail.includes("mix") || normalizedEmail.includes("pro")) {
    return {
      activationLimitPerProduct: 2,
      email: normalizedEmail,
      entitledProductIds: allowed(MIX_BUNDLE),
      planLabel: "Mix Bundle",
    };
  }

  return {
    activationLimitPerProduct: 2,
    email: normalizedEmail,
    entitledProductIds: allowed(STARTER_BUNDLE),
    planLabel: "Starter Bundle",
  };
}
