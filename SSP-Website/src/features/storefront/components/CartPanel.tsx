import { useState } from "react";
import { Button } from "@/components/ui/Button";
import { useStorefront } from "@/features/storefront/StorefrontProvider";

type CheckoutState = {
  orderNumber: string;
  email: string;
};

function formatMoney(value: number) {
  return `$${value.toFixed(2)}`;
}

function createOrderNumber() {
  const serial = Math.floor(1000 + Math.random() * 9000);
  return `SSP-TEST-${serial}`;
}

export function CartPanel() {
  const { cartLines, clearCart, decrementItem, incrementItem, itemCount, removeItem, subtotal } =
    useStorefront();
  const [checkoutState, setCheckoutState] = useState<CheckoutState | null>(null);

  function handleSubmit(formData: FormData) {
    const email = String(formData.get("email") ?? "");
    const orderNumber = createOrderNumber();
    setCheckoutState({ orderNumber, email });
    clearCart();
  }

  return (
    <aside className="cart-panel" id="cart">
      <div className="cart-panel__header">
        <span className="cart-panel__eyebrow">Cart / Dummy Checkout</span>
        <h3>{itemCount > 0 ? `${itemCount} item${itemCount === 1 ? "" : "s"} ready` : "Cart is empty"}</h3>
        <p>
          This is a test storefront flow. It behaves like checkout, but it never charges anything.
        </p>
      </div>

      <div className="cart-panel__lines">
        {cartLines.length > 0 ? (
          cartLines.map((line) => (
            <article key={line.product.slug} className="cart-line">
              <img alt={`${line.product.name} cover`} src={line.product.image} />
              <div className="cart-line__body">
                <strong>{line.product.name}</strong>
                <span>{formatMoney(line.product.priceUsd)} each</span>
                <div className="cart-line__controls">
                  <button onClick={() => decrementItem(line.product.slug)}>-</button>
                  <span>{line.quantity}</span>
                  <button onClick={() => incrementItem(line.product.slug)}>+</button>
                  <button onClick={() => removeItem(line.product.slug)}>Remove</button>
                </div>
              </div>
              <strong className="cart-line__total">{formatMoney(line.lineTotal)}</strong>
            </article>
          ))
        ) : (
          <div className="cart-panel__empty">
            <p>Add any available plugin to start the test checkout flow.</p>
          </div>
        )}
      </div>

      <div className="cart-panel__summary">
        <div>
          <span>Subtotal</span>
          <strong>{formatMoney(subtotal)}</strong>
        </div>
        <div>
          <span>Delivery</span>
          <strong>Instant digital</strong>
        </div>
        <div>
          <span>Checkout mode</span>
          <strong>Dummy / no charge</strong>
        </div>
      </div>

      <form
        className="checkout-form"
        onSubmit={(event) => {
          event.preventDefault();

          if (cartLines.length === 0) {
            return;
          }

          handleSubmit(new FormData(event.currentTarget));
          event.currentTarget.reset();
        }}
      >
        <div className="checkout-form__grid">
          <label>
            Name
            <input name="name" placeholder="Greg Jones" required />
          </label>
          <label>
            Email
            <input name="email" placeholder="greg@example.com" required type="email" />
          </label>
        </div>

        <label>
          Studio / Company
          <input name="company" placeholder="Optional" />
        </label>

        <label>
          Payment Method
          <select defaultValue="card" name="paymentMethod">
            <option value="card">Credit card (test)</option>
            <option value="paypal">PayPal (test)</option>
            <option value="invoice">Manual invoice (test)</option>
          </select>
        </label>

        <label>
          Order Notes
          <textarea
            name="notes"
            placeholder="Bundle request, beta access, or platform notes"
            rows={4}
          />
        </label>

        <Button disabled={cartLines.length === 0} type="submit">
          Place Test Order
        </Button>
      </form>

      {checkoutState ? (
        <div className="checkout-success">
          <span className="checkout-success__eyebrow">Order simulated</span>
          <h4>{checkoutState.orderNumber}</h4>
          <p>
            Dummy checkout complete for {checkoutState.email}. No payment was processed, but this is
            where a real provider hook would hand off the order.
          </p>
        </div>
      ) : null}
    </aside>
  );
}
