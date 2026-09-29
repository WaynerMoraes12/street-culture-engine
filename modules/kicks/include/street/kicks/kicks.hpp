#pragma once
// kicks: a sneaker resale exchange. Umbrella header for the whole module.
//
//   types.hpp       Price (integer cents), Side, Trade, Level
//   order_book.hpp  price-time priority limit order book
//   catalog.hpp     the sneakers (classic_catalog) and the hype-to-premium curve
//   hype.hpp        Ornstein-Uhlenbeck hype process behind fair values
//   market.hpp      agent-based market: drops, shocks, daily candles, news
//   render.hpp      candlesticks, depth ladder, ticker, news, catalog, ASCII sneaker
//   bench.hpp       matching-engine benchmark

#include "street/kicks/bench.hpp"
#include "street/kicks/catalog.hpp"
#include "street/kicks/hype.hpp"
#include "street/kicks/market.hpp"
#include "street/kicks/order_book.hpp"
#include "street/kicks/render.hpp"
#include "street/kicks/types.hpp"
