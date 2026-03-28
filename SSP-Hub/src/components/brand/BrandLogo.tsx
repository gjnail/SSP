type BrandLogoProps = {
  className?: string;
  showWordmark?: boolean;
};

const schoolSPath =
  "M10 11H31L40 20H24L18 26H34L44 36H27L18 45H37L28 54H7L16 45H32L38 39H21L11 29H28L34 21H19Z";
const schoolPPath =
  "M8 54V11H33L46 24V34L34 46H20V54H8ZM20 23V34H29L34 29V27L29 23H20Z";

export function BrandLogo({ className = "", showWordmark = true }: BrandLogoProps) {
  return (
    <div className={`brand-lockup ${className}`.trim()}>
      <svg className="brand-mark" viewBox="0 0 176 64" aria-hidden="true">
        <g className="brand-mark__letter brand-mark__letter--gold">
          <path d={schoolSPath} />
        </g>
        <g className="brand-mark__letter brand-mark__letter--cyan" transform="translate(52 0)">
          <path d={schoolSPath} />
        </g>
        <g className="brand-mark__letter brand-mark__letter--ember" transform="translate(104 0)">
          <path d={schoolPPath} />
        </g>
      </svg>

      {showWordmark ? (
        <div className="brand-wordmark">
          <span className="brand-wordmark__eyebrow">Stupid Simple</span>
          <span className="brand-wordmark__title">Plugins</span>
        </div>
      ) : null}
    </div>
  );
}
