
import argparse
import datetime
import os
from decimal import Decimal, InvalidOperation, ROUND_DOWN

from alpaca.trading.client import TradingClient
from alpaca.trading.enums import (
    AssetStatus,
    ContractType,
    OrderClass,
    OrderSide,
    OrderType,
    TimeInForce,
)
from alpaca.trading.requests import (
    GetOptionContractsRequest,
    LimitOrderRequest,
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


def find_protective_option_contract(
    client: TradingClient,
    symbol: str,
    direction: str,
    take_profit: float,
    stop_loss: float,
    target_dte: int = 21,
):
    """
    Find the best active, tradable protective option contract:
    - BUY (long stock)  -> Protective PUT with strike near stop_loss
    - SELL (short stock) -> Protective CALL with strike near stop_loss
    """
    option_type = ContractType.PUT if direction == "BUY" else ContractType.CALL
    today = datetime.date.today()

    windows = [
        (today + datetime.timedelta(days=7), today + datetime.timedelta(days=45)),
        (today + datetime.timedelta(days=1), today + datetime.timedelta(days=90)),
    ]

    contracts = []
    for exp_gte, exp_lte in windows:
        req = GetOptionContractsRequest(
            underlying_symbols=[symbol.upper()],
            status=AssetStatus.ACTIVE,
            type=option_type,
            expiration_date_gte=exp_gte,
            expiration_date_lte=exp_lte,
            limit=1000,
        )
        resp = client.get_option_contracts(req)
        raw_list = (
            resp.option_contracts
            if hasattr(resp, "option_contracts") and resp.option_contracts
            else (resp.get("option_contracts", []) if isinstance(resp, dict) else [])
        )
        contracts = [c for c in raw_list if getattr(c, "tradable", True)]
        if contracts:
            break

    if not contracts:
        return None

    def score_contract(contract):
        strike = float(contract.strike_price)
        exp_date = contract.expiration_date
        if isinstance(exp_date, str):
            exp_date = datetime.date.fromisoformat(exp_date[:10])
        dte = (exp_date - today).days

        # Prefer strikes at or just beyond stop_loss for cheaper gap insurance
        if direction == "SELL":
            beyond_sl = 0 if (strike >= stop_loss) else 1
        else:
            beyond_sl = 0 if (strike <= stop_loss) else 1

        strike_dist = abs(strike - stop_loss)
        dte_dist = abs(dte - target_dte)
        return (beyond_sl, strike_dist, dte_dist)

    contracts.sort(key=score_contract)
    return contracts[0]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Submit a paper market order with attached bracket exits and a protective option."
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
    parser.add_argument(
        "--option-dte",
        type=int,
        default=21,
        help="Target days to expiration for the protective option (default: 21).",
    )
    parser.add_argument(
        "--no-protective-option",
        action="store_true",
        help="Skip submitting the protective Call/Put option order.",
    )
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
        symbol=args.symbol.upper(),
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

    if not args.no_protective_option:
        contract = find_protective_option_contract(
            client=client,
            symbol=args.symbol.upper(),
            direction=args.direction,
            take_profit=float(args.take_profit),
            stop_loss=float(args.stop_loss),
            target_dte=args.option_dte,
        )
        if contract is None:
            print(
                f"Warning: No active tradable protective option found for {args.symbol.upper()}."
            )
        else:
            contract_size = int(float(getattr(contract, "size", 100) or 100))
            option_qty = max(1, round(args.quantity / contract_size))
            clock = client.get_clock()
            close_px = getattr(contract, "close_price", None)
            if clock.is_open or close_px is None or float(close_px) <= 0:
                option_order = MarketOrderRequest(
                    symbol=contract.symbol,
                    qty=option_qty,
                    side=OrderSide.BUY,
                    type=OrderType.MARKET,
                    time_in_force=TimeInForce.DAY,
                )
            else:
                limit_px = round(float(close_px) * 1.05, 2)
                option_order = LimitOrderRequest(
                    symbol=contract.symbol,
                    qty=option_qty,
                    side=OrderSide.BUY,
                    type=OrderType.LIMIT,
                    time_in_force=TimeInForce.DAY,
                    limit_price=max(0.05, limit_px),
                )
            submitted_option = client.submit_order(order_data=option_order)
            print(
                f"Protective {contract.type.value.upper()} option order submitted: "
                f"{contract.symbol} x{option_qty} "
                f"(strike=${float(contract.strike_price):.2f}, exp={contract.expiration_date}) -> "
                f"{submitted_option.id} ({submitted_option.status})"
            )