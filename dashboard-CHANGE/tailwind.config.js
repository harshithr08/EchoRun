/** @type {import('tailwindcss').Config} */
export default {
  content: ["./index.html", "./src/**/*.{js,jsx}"],
  theme: {
    extend: {
      colors: {
        ink: "#1d2738",
        mist: "#f5f7fb",
        paper: "#fcfcfe",
        accent: "#2f6df6",
        calm: "#7f8daa",
        border: "#dce2ee",
      },
      boxShadow: {
        panel: "0 8px 24px rgba(31, 51, 84, 0.06)",
      },
      fontFamily: {
        mono: ['"IBM Plex Mono"', '"JetBrains Mono"', '"SFMono-Regular"', "monospace"],
      },
      animation: {
        rise: "rise 480ms ease-out both",
        glow: "glow 1.8s ease-in-out infinite alternate",
      },
      keyframes: {
        rise: {
          "0%": { opacity: "0", transform: "translateY(10px)" },
          "100%": { opacity: "1", transform: "translateY(0)" },
        },
        glow: {
          "0%": { boxShadow: "0 0 0 rgba(47, 109, 246, 0.10)" },
          "100%": { boxShadow: "0 0 24px rgba(47, 109, 246, 0.24)" },
        },
      },
    },
  },
  plugins: [],
};
