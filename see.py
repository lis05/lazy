#!/usr/bin/env python3

import sys
import os
import pandas as pd
import numpy as np
import plotly.express as px

try:
    from dash import Dash, dcc, html, Input, Output, ctx, dash_table
except ImportError:
    print("Error: 'dash' is required. Install with: pip install dash pandas plotly")
    sys.exit(1)


def load_data(csv_path):
    if not os.path.isfile(csv_path):
        print(f"Error: File '{csv_path}' does not exist.")
        sys.exit(1)

    try:
        df = pd.read_csv(csv_path)
    except Exception as e:
        print(f"Error reading CSV: {e}")
        sys.exit(1)

    if "filename" not in df.columns:
        print("Error: CSV must contain a 'filename' column.")
        sys.exit(1)

    if "enc_time" in df.columns:
        df["log_enc_time"] = np.log10(df["enc_time"].replace(0, np.nan))
    if "dec_time" in df.columns:
        df["log_dec_time"] = np.log10(df["dec_time"].replace(0, np.nan))

    return df


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <results.csv>")
        sys.exit(1)

    CSV_PATH = sys.argv[1]
    df = load_data(CSV_PATH)

    metrics = [
        "ratio_percent",
        "enc_time",
        "log_enc_time",
        "dec_time",
        "log_dec_time",
        "enc_size",
        "orig_size",
    ]
    available_metrics = [m for m in metrics if m in df.columns]

    initial_x = "log_enc_time" if "log_enc_time" in df.columns else available_metrics[0]
    initial_y = "ratio_percent" if "ratio_percent" in df.columns else available_metrics[0]

    app = Dash(__name__, suppress_callback_exceptions=True)

    all_files = list(df["filename"].unique())
    default_files = [f for f in all_files if f != "AVERAGE"]

    app.layout = html.Div(
        [
            dcc.Interval(id="interval-component", interval=10000, n_intervals=0),
            html.H2(
                "Compressor Benchmark Viewer", style={"fontFamily": "sans-serif"}
            ),
            html.Div(
                [
                    html.Div(
                        [
                            html.Label(
                                "X Axis",
                                style={"fontWeight": "bold", "display": "block"},
                            ),
                            dcc.Dropdown(
                                id="x-axis",
                                options=available_metrics,
                                value=initial_x,
                                clearable=False,
                            ),
                        ],
                        style={
                            "width": "200px",
                            "display": "inline-block",
                            "marginRight": "20px",
                        },
                    ),
                    html.Div(
                        [
                            html.Label(
                                "Y Axis",
                                style={"fontWeight": "bold", "display": "block"},
                            ),
                            dcc.Dropdown(
                                id="y-axis",
                                options=available_metrics,
                                value=initial_y,
                                clearable=False,
                            ),
                        ],
                        style={
                            "width": "200px",
                            "display": "inline-block",
                            "marginRight": "20px",
                        },
                    ),
                ],
                style={"paddingBottom": "20px", "fontFamily": "sans-serif"},
            ),
            html.Div(
                [
                    html.Div(
                        [
                            html.Label(
                                "Active Datasets:",
                                style={
                                    "fontWeight": "bold",
                                    "display": "inline-block",
                                    "marginRight": "10px",
                                },
                            ),
                            html.Button(
                                "Enable All but AVERAGE",
                                id="btn-select-no-avg",
                                style={
                                    "padding": "4px 8px",
                                    "cursor": "pointer",
                                    "backgroundColor": "#f0f0f0",
                                    "border": "1px solid #ccc",
                                    "borderRadius": "4px",
                                    "marginRight": "10px",
                                },
                            ),
                            html.Button(
                                "Show Only AVERAGE",
                                id="btn-select-only-avg",
                                style={
                                    "padding": "4px 8px",
                                    "cursor": "pointer",
                                    "backgroundColor": "#f0f0f0",
                                    "border": "1px solid #ccc",
                                    "borderRadius": "4px",
                                },
                            ),
                        ],
                        style={
                            "display": "flex",
                            "alignItems": "center",
                            "marginBottom": "5px",
                        },
                    ),
                    dcc.Checklist(
                        id="dataset-filter",
                        options=[{"label": f, "value": f} for f in all_files],
                        value=default_files,
                        inline=True,
                        inputStyle={"marginRight": "5px", "marginLeft": "10px"},
                    ),
                ],
                style={"paddingBottom": "10px", "fontFamily": "sans-serif"},
            ),
            dcc.Graph(id="scatter-plot", style={"height": "75vh"}),
            html.Div(
                [
                    html.H3("Benchmark Data Table", style={"fontFamily": "sans-serif"}),
                    dash_table.DataTable(
                        id="data-table",
                        page_action="none",
                        style_table={"overflowX": "auto"},
                        style_cell={
                            "textAlign": "left",
                            "fontFamily": "sans-serif",
                            "padding": "8px",
                        },
                        style_header={
                            "backgroundColor": "#f4f4f4",
                            "fontWeight": "bold",
                            "border": "1px solid #ccc",
                        },
                        style_data={
                            "border": "1px solid #e0e0e0",
                        },
                    ),
                ],
                style={"paddingTop": "20px"},
            ),
        ],
        style={"padding": "20px"},
    )

    @app.callback(
        Output("dataset-filter", "value"),
        Input("btn-select-no-avg", "n_clicks"),
        Input("btn-select-only-avg", "n_clicks"),
        prevent_initial_call=True,
    )
    def update_selection_buttons(btn_no_avg, btn_only_avg):
        triggered_id = ctx.triggered_id
        if triggered_id == "btn-select-no-avg":
            return default_files
        elif triggered_id == "btn-select-only-avg":
            return ["AVERAGE"] if "AVERAGE" in all_files else []
        return default_files

    @app.callback(
        Output("scatter-plot", "figure"),
        Output("data-table", "data"),
        Output("data-table", "columns"),
        Input("x-axis", "value"),
        Input("y-axis", "value"),
        Input("dataset-filter", "value"),
        Input("interval-component", "n_intervals"),
    )
    def update_dashboard(x_col, y_col, selected_datasets, n_intervals):
        df_current = load_data(CSV_PATH)
        filtered_df = df_current[df_current["filename"].isin(selected_datasets)].copy()

        if filtered_df.empty:
            return px.scatter(title="No data selected"), [], []

        filtered_df.sort_values(
            by=["compressor", "filename", "ratio_percent"], inplace=True
        )

        fig = px.line(
            filtered_df,
            x=x_col,
            y=y_col,
            color="compressor",
            line_group="filename",
            markers=True,
            template="plotly_white",
            hover_data=["level"] + available_metrics,
        )

        fig.update_traces(
            line=dict(width=1, dash="dot"),
            marker=dict(symbol="circle-open", size=7, line=dict(width=1)),
        )

        fig.update_layout(margin=dict(t=20, r=20, l=40, b=40))

        ratio_col = "ratio_percent" if "ratio_percent" in filtered_df.columns else "ratio"
        target_cols = ["compressor", "level", "enc_time", "dec_time", ratio_col]
        display_cols = [c for c in target_cols if c in filtered_df.columns]

        if ratio_col in filtered_df.columns:
            table_df = filtered_df.sort_values(by=ratio_col, ascending=True)
        else:
            table_df = filtered_df

        table_data = table_df[display_cols].to_dict("records")
        table_columns = [{"name": c, "id": c} for c in display_cols]

        return fig, table_data, table_columns


    print("Starting dashboard server at http://127.0.0.1:8050/")
    app.run(debug=False)
