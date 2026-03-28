import {
  createContext,
  useContext,
  useReducer,
  type PropsWithChildren,
} from "react";
import { products, type Product } from "@/features/home/content";

type CartMap = Record<string, number>;

type StorefrontState = {
  cart: CartMap;
};

type StorefrontAction =
  | { type: "add"; slug: string }
  | { type: "increment"; slug: string }
  | { type: "decrement"; slug: string }
  | { type: "remove"; slug: string }
  | { type: "clear" };

type CartLine = {
  product: Product;
  quantity: number;
  lineTotal: number;
};

type StorefrontValue = {
  cart: CartMap;
  cartLines: CartLine[];
  itemCount: number;
  subtotal: number;
  addToCart: (slug: string) => void;
  incrementItem: (slug: string) => void;
  decrementItem: (slug: string) => void;
  removeItem: (slug: string) => void;
  clearCart: () => void;
  getQuantity: (slug: string) => number;
};

const initialState: StorefrontState = {
  cart: {},
};

const StorefrontContext = createContext<StorefrontValue | null>(null);

function isPurchasable(product: Product | undefined) {
  return product?.status === "available";
}

function storefrontReducer(state: StorefrontState, action: StorefrontAction): StorefrontState {
  const nextCart = { ...state.cart };

  switch (action.type) {
    case "add":
    case "increment": {
      const product = products.find((entry) => entry.slug === action.slug);

      if (!isPurchasable(product)) {
        return state;
      }

      nextCart[action.slug] = (nextCart[action.slug] ?? 0) + 1;
      return { cart: nextCart };
    }

    case "decrement": {
      const current = nextCart[action.slug] ?? 0;

      if (current <= 1) {
        delete nextCart[action.slug];
      } else {
        nextCart[action.slug] = current - 1;
      }

      return { cart: nextCart };
    }

    case "remove": {
      delete nextCart[action.slug];
      return { cart: nextCart };
    }

    case "clear":
      return initialState;

    default:
      return state;
  }
}

export function StorefrontProvider({ children }: PropsWithChildren) {
  const [state, dispatch] = useReducer(storefrontReducer, initialState);

  const cartLines = products
    .filter((product) => (state.cart[product.slug] ?? 0) > 0)
    .map((product) => {
      const quantity = state.cart[product.slug] ?? 0;

      return {
        product,
        quantity,
        lineTotal: product.priceUsd * quantity,
      };
    });

  const itemCount = cartLines.reduce((total, line) => total + line.quantity, 0);
  const subtotal = cartLines.reduce((total, line) => total + line.lineTotal, 0);

  const value: StorefrontValue = {
    cart: state.cart,
    cartLines,
    itemCount,
    subtotal,
    addToCart: (slug) => dispatch({ type: "add", slug }),
    incrementItem: (slug) => dispatch({ type: "increment", slug }),
    decrementItem: (slug) => dispatch({ type: "decrement", slug }),
    removeItem: (slug) => dispatch({ type: "remove", slug }),
    clearCart: () => dispatch({ type: "clear" }),
    getQuantity: (slug) => state.cart[slug] ?? 0,
  };

  return <StorefrontContext.Provider value={value}>{children}</StorefrontContext.Provider>;
}

export function useStorefront() {
  const value = useContext(StorefrontContext);

  if (!value) {
    throw new Error("useStorefront must be used inside StorefrontProvider.");
  }

  return value;
}
