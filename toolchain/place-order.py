
import argparse
import os
from decimal import Decimal, InvalidOperation, ROUND_DOWN

from alpaca.trading.client import TradingClient
from alpaca.trading.enums import OrderClass, OrderSide, TimeInForce, OrderType
from alpaca.trading.requests import (
    MarketOrderRequest,
    StopLossRequest,
    TakeProfitRequest,
)


def positive_price(value):
    try:
        price = Decimal(value)
    except InvalidOperation as exc:
        raise argparse.ArgumentTypeError("must be a number") from exc
    if not price.is_finite() or price <= 0:
        raise argparse.ArgumentTypeError("must be a finite number greater than zero")
    return price


def truncate_to_cent(price):
    return price.quantize(Decimal("0.01"), rounding=ROUND_DOWN)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Submit a paper market order with attached bracket exits."
    )
    parser.add_argument("symbol")
    parser.add_argument(
        "direction",
        type=str.upper,
        choices=("BUY", "SELL"),
        metavar="direction",
    )
    parser.add_argument("quantity", type=int, help="number of whole shares")
    parser.add_argument("take_profit", type=positive_price, metavar="take-profit")
    parser.add_argument("stop_loss", type=positive_price, metavar="stop-loss")
    args = parser.parse_args()

    args.take_profit = truncate_to_cent(args.take_profit)
    args.stop_loss = truncate_to_cent(args.stop_loss)
    if args.take_profit <= 0 or args.stop_loss <= 0:
        parser.error("take-profit and stop-loss must be at least $0.01")
    if args.quantity <= 0:
        parser.error("quantity must be a positive whole number")
    if args.direction == "BUY" and args.take_profit <= args.stop_loss:
        parser.error("for BUY, take-profit must be greater than stop-loss")
    if args.direction == "SELL" and args.take_profit >= args.stop_loss:
        parser.error("for SELL, take-profit must be less than stop-loss")

    api_key = os.environ.get("ALPACA_API_KEY")
    secret_key = os.environ.get("ALPACA_API_SECRET")

    client = TradingClient(api_key, secret_key, paper=True)
    order = MarketOrderRequest(
        symbol=args.symbol,
        qty=args.quantity,
        side=OrderSide(args.direction.lower()),
        type=OrderType.MARKET,
        time_in_force=TimeInForce.GTC,
        order_class=OrderClass.BRACKET,
        take_profit=TakeProfitRequest(limit_price=float(args.take_profit)),
        stop_loss=StopLossRequest(stop_price=float(args.stop_loss)),
    )
    submitted_order = client.submit_order(order_data=order)
    print(
        f"Paper bracket order submitted: "
        f"{submitted_order.id} ({submitted_order.status})"
    )