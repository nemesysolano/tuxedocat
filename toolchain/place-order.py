
import argparse
import datetime
import math
import os
import sys
from decimal import Decimal, InvalidOperation, ROUND_DOWN

from alpaca.data.historical import OptionHistoricalDataClient
from alpaca.data.requests import OptionLatestQuoteRequest
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


def verify_equity_liquidity_and_borrow(
    client: TradingClient,
    symbol: str,
    direction: str,
    allow_htb: bool = False,
):
    """
    Safeguard 1: Verify the equity asset is active/tradable and, for SELL (short) orders,
    confirm it is shortable and Easy-to-Borrow (ETB) before submitting.
    """
    asset = client.get_asset(symbol.upper())
    if not getattr(asset, "tradable", False):
        raise RuntimeError(f"Asset {symbol.upper()} is not tradable on Alpaca.")

    if direction == "SELL":
        if not getattr(asset, "shortable", False):
            raise RuntimeError(
                f"Liquidity/Borrow check failed: {symbol.upper()} is not shortable on Alpaca."
            )
        if not getattr(asset, "easy_to_borrow", False) and not allow_htb:
            raise RuntimeError(
                f"Liquidity/Borrow check failed: {symbol.upper()} is Hard-to-Borrow (not easy_to_borrow). "
                f"Pass --allow-htb to override."
            )


def find_protective_option_contract(
    client: TradingClient,
    symbol: str,
    direction: str,
    take_profit: float,
    stop_loss: float,
    option_qty: int = 1,
    target_dte: int = 21,
    min_open_interest: int = 10,
):
    """
    Safeguard 2: Find the best active, tradable protective option contract while filtering
    and ranking by open_interest so thinly traded contracts are avoided:
    - BUY (long stock)   -> Protective PUT with strike <= stop_loss
    - SELL (short stock) -> Protective CALL with strike >= stop_loss
    """
    option_type = ContractType.PUT if direction == "BUY" else ContractType.CALL
    today = datetime.date.today()
    required_oi = max(min_open_interest, option_qty * 2)

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
        raw_oi = getattr(contract, "open_interest", None)
        oi = int(float(raw_oi)) if raw_oi is not None else 0

        # Prefer strikes at or just beyond stop_loss for cheaper gap insurance
        if direction == "SELL":
            beyond_sl = 0 if (strike >= stop_loss) else 1
        else:
            beyond_sl = 0 if (strike <= stop_loss) else 1

        # Prefer contracts meeting minimum open interest threshold
        low_oi_penalty = 0 if (oi >= required_oi) else 1
        strike_dist = round(abs(strike - stop_loss), 4)
        dte_dist = abs(dte - target_dte)
        return (beyond_sl, low_oi_penalty, strike_dist, dte_dist, -oi)

    contracts.sort(key=score_contract)
    return contracts[0]


def compute_protective_option_limit_price(
    api_key: str,
    secret_key: str,
    contract,
    take_profit: float,
    stop_loss: float,
) -> float:
    """
    Safeguard 3: Always compute a capped LIMIT price for the option leg using live/latest
    NBBO quotes (with fallback to contract close_price) so wide option spreads are never
    swept with a MARKET order.
    """
    bid_px = 0.0
    ask_px = 0.0
    try:
        opt_data_client = OptionHistoricalDataClient(api_key, secret_key)
        quotes = opt_data_client.get_option_latest_quote(
            OptionLatestQuoteRequest(symbol_or_symbols=contract.symbol)
        )
        quote = quotes.get(contract.symbol) if isinstance(quotes, dict) else None
        if quote is not None:
            bid_px = float(getattr(quote, "bid_price", 0.0) or 0.0)
            ask_px = float(getattr(quote, "ask_price", 0.0) or 0.0)
    except Exception:
        pass

    close_raw = getattr(contract, "close_price", None)
    close_px = float(close_raw) if (close_raw is not None and float(close_raw) > 0) else 0.0

    # Cap maximum acceptable premium at 40% of the equity TP-to-SL band so TP stays net-positive
    band_cap = max(0.05, round(abs(take_profit - stop_loss) * 0.40, 2))

    if ask_px > 0.0:
        if bid_px > 0.0:
            mid_px = 0.5 * (bid_px + ask_px)
            spread_pct = (ask_px - bid_px) / ask_px
            if spread_pct > 0.35:
                print(
                    f"Liquidity note: Wide option spread on {contract.symbol} "
                    f"(bid=${bid_px:.2f}, ask=${ask_px:.2f}, spread={spread_pct*100:.1f}%); "
                    f"capping limit price near mid."
                )
            raw_limit = min(ask_px, mid_px * 1.15)
        else:
            raw_limit = ask_px
        return max(0.05, round(min(raw_limit, band_cap), 2))

    if close_px > 0.0:
        return max(0.05, round(min(close_px * 1.05, band_cap), 2))

    return max(0.05, round(band_cap * 0.50, 2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Submit a paper market order with attached bracket exits, liquidity checks, and a protective option."
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
        "--min-open-interest",
        type=int,
        default=10,
        help="Minimum preferred open interest for the protective option (default: 10).",
    )
    parser.add_argument(
        "--allow-htb",
        action="store_true",
        help="Allow shorting Hard-to-Borrow (non-ETB) symbols.",
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

    # Safeguard 1: Check short borrow / ETB status before submitting
    try:
        verify_equity_liquidity_and_borrow(
            client=client,
            symbol=args.symbol.upper(),
            direction=args.direction,
            allow_htb=args.allow_htb,
        )
    except RuntimeError as err:
        print(f"Error: {err}", file=sys.stderr)
        sys.exit(1)

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
        est_option_qty = max(1, round(args.quantity / 100))
        contract = find_protective_option_contract(
            client=client,
            symbol=args.symbol.upper(),
            direction=args.direction,
            take_profit=float(args.take_profit),
            stop_loss=float(args.stop_loss),
            option_qty=est_option_qty,
            target_dte=args.option_dte,
            min_open_interest=args.min_open_interest,
        )
        if contract is None:
            print(
                f"Warning: No active tradable protective option found for {args.symbol.upper()}."
            )
        else:
            contract_size = int(float(getattr(contract, "size", 100) or 100))
            option_qty = max(1, round(args.quantity / contract_size))
            raw_oi = getattr(contract, "open_interest", None)
            oi = int(float(raw_oi)) if raw_oi is not None else 0
            required_oi = max(args.min_open_interest, option_qty * 2)
            if oi < required_oi:
                print(
                    f"Liquidity warning: {contract.symbol} open_interest={oi} is below "
                    f"recommended threshold ({required_oi}) for {option_qty} contracts."
                )

            # Safeguard 3: Always submit a capped LIMIT order for the option leg
            limit_px = compute_protective_option_limit_price(
                api_key=api_key,
                secret_key=secret_key,
                contract=contract,
                take_profit=float(args.take_profit),
                stop_loss=float(args.stop_loss),
            )
            option_order = LimitOrderRequest(
                symbol=contract.symbol,
                qty=option_qty,
                side=OrderSide.BUY,
                type=OrderType.LIMIT,
                time_in_force=TimeInForce.DAY,
                limit_price=limit_px,
            )
            submitted_option = client.submit_order(order_data=option_order)
            print(
                f"Protective {contract.type.value.upper()} option LIMIT order submitted: "
                f"{contract.symbol} x{option_qty} @ limit=${limit_px:.2f} "
                f"(strike=${float(contract.strike_price):.2f}, exp={contract.expiration_date}, OI={oi}) -> "
                f"{submitted_option.id} ({submitted_option.status})"
            )