import type { Config } from "tailwindcss";

const config: Config = {
  content: [
    "./src/pages/**/*.{js,ts,jsx,tsx,mdx}",
    "./src/components/**/*.{js,ts,jsx,tsx,mdx}",
    "./src/app/**/*.{js,ts,jsx,tsx,mdx}",
  ],
  theme: {
    extend: {
      colors: {
        brand: {
          ink: "#12263f",
          navy: "#173754",
          forest: "#15803d",
          mint: "#e8f7ee",
          line: "#d8e0e8",
          cloud: "#f5f8fb",
          text: "#243746",
        },
        status: {
          success: "#166534",
          warning: "#b45309",
          danger: "#b91c1c",
          info: "#1d4ed8",
        },
      },
      boxShadow: {
        panel: "0 18px 40px rgba(18, 38, 63, 0.08)",
        soft: "0 8px 24px rgba(18, 38, 63, 0.06)",
      },
      backgroundImage: {
        "map-grid":
          "linear-gradient(rgba(19,55,84,0.06) 1px, transparent 1px), linear-gradient(90deg, rgba(19,55,84,0.06) 1px, transparent 1px)",
      },
    },
  },
  plugins: [],
};

export default config;
