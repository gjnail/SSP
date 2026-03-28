import { Routes, Route } from "react-router-dom";
import { SiteLayout } from "@/app/SiteLayout";
import { FaqSection } from "@/features/home/sections/FaqSection";
import { StorefrontProvider } from "@/features/storefront/StorefrontProvider";
import { AboutPage } from "@/features/pages/AboutPage";
import { CartPage } from "@/features/pages/CartPage";
import { ContactPage } from "@/features/pages/ContactPage";
import { HomePage } from "@/features/pages/HomePage";
import { HubDownloadPage } from "@/features/pages/HubDownloadPage";
import { HubPage } from "@/features/pages/HubPage";
import { NotFoundPage } from "@/features/pages/NotFoundPage";
import { ProductDetailPage } from "@/features/pages/ProductDetailPage";
import { ProductsPage } from "@/features/pages/ProductsPage";

export function App() {
  return (
    <StorefrontProvider>
      <Routes>
        <Route element={<SiteLayout />} path="/">
          <Route element={<HomePage />} index />
          <Route element={<ProductsPage />} path="products" />
          <Route element={<ProductDetailPage />} path="products/:slug" />
          <Route element={<CartPage />} path="cart" />
          <Route element={<HubPage />} path="hub" />
          <Route element={<HubDownloadPage />} path="hub/download" />
          <Route element={<AboutPage />} path="about" />
          <Route element={<ContactPage />} path="contact" />
          <Route
            element={
              <>
                <FaqSection />
              </>
            }
            path="faq"
          />
          <Route element={<NotFoundPage />} path="*" />
        </Route>
      </Routes>
    </StorefrontProvider>
  );
}
