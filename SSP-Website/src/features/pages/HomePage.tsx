import { FeaturedProductsSection } from "@/features/home/sections/FeaturedProductsSection";
import { HeroSection } from "@/features/home/sections/HeroSection";
import { ManifestoSection } from "@/features/home/sections/ManifestoSection";
import { WorkflowSection } from "@/features/home/sections/WorkflowSection";

export function HomePage() {
  return (
    <>
      <HeroSection />
      <FeaturedProductsSection />
      <WorkflowSection />
      <ManifestoSection />
    </>
  );
}
