import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "Polymarket HFT Monitor",
  description: "C11 Event-Driven Arbitrage & Momentum Dashboard",
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en" className="dark">
      <body className="antialiased bg-[#0b0e14] text-gray-100 min-h-screen">
        {children}
      </body>
    </html>
  );
}