import argparse
import csv
import os
import sys
from collections import defaultdict, deque
from datetime import date
from decimal import Decimal

from alpaca.trading.client import TradingClient


FIELDNAMES = (
    "status",
    "symbol",
    "side",
    "quantity",
    "opening_date",
    "entry_time",
    "exit_time",
    "entry_price",
    "exit_price",
    "current_price",
    "market_value",
    "unrealized_pl",
    "realized_pl",
)


def fetch_fill_activities(client):
    activities = []
    page_token = None

    while True:
        params = {
            "direction": "asc",
            "page_size": 100,
        }
        if page_token is not None:
            params["page_token"] = page_token
        page = client.get(
            "/account/activities/FILL",
            data=params,
        )
        activities.extend(page)
        if len(page) < 100:
            return activities

        next_page_token = page[-1]["id"]
        if next_page_token == page_token:
            raise RuntimeError("Alpaca returned a repeated activity page token")
        page_token = next_page_token


def process_fill_activities(activities):
    fills_by_symbol = defaultdict(list)
    for activity in activities:
        fills_by_symbol[activity["symbol"]].append(activity)

    closed = []
    opening_dates = {}
    for symbol, fills in fills_by_symbol.items():
        fills.sort(
            key=lambda activity: (
                activity["transaction_time"],
                activity["id"],
            )
        )
        open_lots = deque()

        for fill in fills:
            fill_quantity = Decimal(fill["qty"])
            fill_price = Decimal(fill["price"])
            fill_time = fill["transaction_time"]
            if hasattr(fill_time, "isoformat"):
                fill_time = fill_time.isoformat()
            signed_quantity = (
                fill_quantity
                if fill["side"].lower() == "buy"
                else -fill_quantity
            )

            while signed_quantity and open_lots and (
                (signed_quantity > 0) != (open_lots[0]["quantity"] > 0)
            ):
                lot = open_lots[0]
                matched_quantity = min(abs(signed_quantity), abs(lot["quantity"]))
                is_long = lot["quantity"] > 0
                realized_pl = matched_quantity * (
                    fill_price - lot["price"]
                    if is_long
                    else lot["price"] - fill_price
                )
                closed.append(
                    {
                        "status": "closed",
                        "symbol": symbol,
                        "side": "long" if is_long else "short",
                        "quantity": matched_quantity,
                        "opening_date": lot["time"][:10],
                        "entry_time": lot["time"],
                        "exit_time": fill_time,
                        "entry_price": lot["price"],
                        "exit_price": fill_price,
                        "current_price": "",
                        "market_value": "",
                        "unrealized_pl": "",
                        "realized_pl": realized_pl,
                    }
                )

                lot["quantity"] += matched_quantity if not is_long else -matched_quantity
                signed_quantity += matched_quantity if signed_quantity < 0 else -matched_quantity
                if lot["quantity"] == 0:
                    open_lots.popleft()

            if signed_quantity:
                open_lots.append(
                    {
                        "quantity": signed_quantity,
                        "price": fill_price,
                        "time": fill_time,
                    }
                )

        if open_lots:
            opening_dates[symbol] = min(lot["time"][:10] for lot in open_lots)

    return closed, opening_dates


def active_positions(positions, opening_dates):
    rows = []
    for position in positions:
        quantity = Decimal(position.qty)
        rows.append(
            {
                "status": "active",
                "symbol": position.symbol,
                "side": "long" if quantity > 0 else "short",
                "quantity": abs(quantity),
                "opening_date": opening_dates.get(position.symbol, ""),
                "entry_time": "",
                "exit_time": date.today().isoformat(),
                "entry_price": position.avg_entry_price,
                "exit_price": "",
                "current_price": position.current_price,
                "market_value": position.market_value,
                "unrealized_pl": position.unrealized_pl,
                "realized_pl": "",
            }
        )
    return rows


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Print active positions and closed round trips from Alpaca as CSV."
        )
    )
    args = parser.parse_args()

    api_key = os.environ.get("ALPACA_API_KEY")
    secret_key = os.environ.get("ALPACA_API_SECRET")
    if not api_key or not secret_key:
        parser.error("ALPACA_API_KEY and ALPACA_API_SECRET must be set")

    client = TradingClient(api_key, secret_key, paper=True)
    closed, opening_dates = process_fill_activities(fetch_fill_activities(client))
    rows = closed
    rows.extend(active_positions(client.get_all_positions(), opening_dates))
    rows.sort(key=lambda row: (row["symbol"], row["entry_time"], row["status"]))

    writer = csv.DictWriter(sys.stdout, fieldnames=FIELDNAMES, lineterminator="\n")
    writer.writeheader()
    writer.writerows(rows)


if __name__ == "__main__":
    main()