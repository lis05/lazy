#!/bin/env python3
import sys
import os
import pandas as pd
import numpy as np
import plotly.express as px
import plotly.graph_objects as go

try:
    from dash import Dash, dcc, html, Input, Output, State, dash_table, ctx
except ImportError:
    print("Error: 'dash' is required. Install with: pip install dash pandas")
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

    df["log_enc_time"] = np.log10(df["enc_time"].replace(0, np.nan))

    for col in ["-O", "--lazy-matching", "--lm"]:
        if col in df.columns:
            df[col] = df[col].fillna(False).astype(int)

    return df


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <results.csv>")
        sys.exit(1)

    df = load_data(sys.argv[1])

    metrics = ["ratio", "enc_time", "log_enc_time", "dec_time", "enc_size", "orig_size"]
    ignore_cols = metrics + ["filename"]
    parameters = [col for col in df.columns if col not in ignore_cols]

    initial_x = "log_enc_time" if "log_enc_time" in df.columns else (parameters[0] if parameters else df.columns[0])
    initial_y = "ratio" if "ratio" in df.columns else metrics[0]
    initial_color = parameters[1] if len(parameters) > 1 else parameters[0]

    app = Dash(__name__, suppress_callback_exceptions=True)

    all_files = list(df["filename"].unique())
    default_files = [f for f in all_files if f != "AVERAGE"]

    app.layout = html.Div(
        [
            dcc.Download(id="download-pareto-csv"),
            html.H2(
                "Interactive LZ77 Benchmark Explorer",
                style={"fontFamily": "sans-serif"},
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
                                options=parameters + metrics,
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
                                options=metrics + parameters,
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
                    html.Div(
                        [
                            html.Label(
                                "Color Scaling",
                                style={"fontWeight": "bold", "display": "block"},
                            ),
                            dcc.Dropdown(
                                id="color-axis",
                                options=parameters,
                                value=initial_color,
                                clearable=False,
                            ),
                        ],
                        style={"width": "200px", "display": "inline-block"},
                    ),
                ],
                style={"paddingBottom": "20px", "fontFamily": "sans-serif"},
            ),
            html.Div(
                [
                    html.Div(
                        [
                            html.Label(
                                "Active Datasets (Recalculates Pareto):",
                                style={"fontWeight": "bold", "display": "inline-block", "marginRight": "10px"},
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
                        style={"display": "flex", "alignItems": "center", "marginBottom": "5px"},
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
            dcc.Graph(id="scatter-plot", style={"height": "60vh"}),
            html.Div(
                id="pareto-table-container",
                style={"paddingTop": "20px", "fontFamily": "sans-serif"},
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
        Output("pareto-table-container", "children"),
        Input("x-axis", "value"),
        Input("y-axis", "value"),
        Input("color-axis", "value"),
        Input("dataset-filter", "value"),
    )
    def update_dashboard(x_col, y_col, color_col, selected_datasets):
        filtered_df = df[df["filename"].isin(selected_datasets)]

        if filtered_df.empty:
            return go.Figure(), html.Div()

        fig = px.scatter(
            filtered_df,
            x=x_col,
            y=y_col,
            color=color_col,
            symbol="filename",
            template="plotly_white",
            color_continuous_scale="viridis",
            custom_data=list(filtered_df.columns),
        )

        hover_lines = [
            f"{col}=%{{customdata[{i}]}}" for i, col in enumerate(filtered_df.columns)
        ]
        hovertemplate = "<br>".join(hover_lines) + "<extra></extra>"

        fig.update_traces(
            hovertemplate=hovertemplate,
            marker=dict(
                size=11, opacity=0.8, line=dict(width=1, color="DarkSlateGrey")
            ),
        )

        pareto_conditions = (x_col == "log_enc_time" and y_col == "ratio") or (
            x_col == "ratio" and y_col == "log_enc_time"
        )

        pareto_table_ui = html.Div(
            [
                html.H4(
                    "Pareto Front Points (Requires ratio and log_enc_time axes)",
                    style={"color": "grey"},
                )
            ]
        )

        if pareto_conditions:
            sorted_df = filtered_df.sort_values(by=[x_col, y_col])
            pareto_front = []
            min_y = float("inf")

            for _, row in sorted_df.iterrows():
                if row[y_col] < min_y:
                    pareto_front.append(row)
                    min_y = row[y_col]

            if pareto_front:
                pareto_df = pd.DataFrame(pareto_front)
                fig.add_trace(
                    go.Scatter(
                        x=pareto_df[x_col],
                        y=pareto_df[y_col],
                        mode="lines",
                        name="Pareto Front",
                        line=dict(color="red", width=2, dash="dash"),
                        hoverinfo="skip",
                        showlegend=False,
                    )
                )

                pareto_table_ui = html.Div(
                    [
                        html.Div(
                            [
                                html.H4(
                                    "Pareto Front Datapoints",
                                    style={"margin": "0px", "display": "inline-block"},
                                ),
                                html.Button(
                                    "Export to CSV",
                                    id="btn-export-csv",
                                    style={
                                        "float": "right",
                                        "padding": "6px 12px",
                                        "cursor": "pointer",
                                        "backgroundColor": "#007BFF",
                                        "color": "white",
                                        "border": "none",
                                        "borderRadius": "4px",
                                        "fontWeight": "bold",
                                    },
                                ),
                            ],
                            style={"marginBottom": "15px", "overflow": "hidden"},
                        ),
                        dash_table.DataTable(
                            data=pareto_df.to_dict("records"),
                            columns=[{"name": i, "id": i} for i in pareto_df.columns],
                            page_action="none",
                            style_table={"overflowX": "auto"},
                            style_cell={
                                "textAlign": "left",
                                "padding": "8px",
                                "fontFamily": "sans-serif",
                            },
                            style_header={
                                "backgroundColor": "#f4f4f4",
                                "fontWeight": "bold",
                            },
                        ),
                    ]
                )

        fig.update_layout(margin=dict(t=20, r=20, l=40, b=40), showlegend=False)

        return fig, pareto_table_ui

    @app.callback(
        Output("download-pareto-csv", "data"),
        Input("btn-export-csv", "n_clicks"),
        State("x-axis", "value"),
        State("y-axis", "value"),
        State("dataset-filter", "value"),
        prevent_initial_call=True,
    )
    def generate_csv(n_clicks, x_col, y_col, selected_datasets):
        if not n_clicks:
            return None

        filtered_df = df[df["filename"].isin(selected_datasets)]
        pareto_conditions = (x_col == "log_enc_time" and y_col == "ratio") or (
            x_col == "ratio" and y_col == "log_enc_time"
        )

        if pareto_conditions and not filtered_df.empty:
            sorted_df = filtered_df.sort_values(by=[x_col, y_col])
            pareto_front = []
            min_y = float("inf")

            for _, row in sorted_df.iterrows():
                if row[y_col] < min_y:
                    pareto_front.append(row)
                    min_y = row[y_col]

            if pareto_front:
                pareto_df = pd.DataFrame(pareto_front)
                return dcc.send_data_frame(
                    pareto_df.to_csv, "pareto_front.csv", index=False
                )
        return None

    print("Starting dashboard server at http://127.0.0.1:8050/")
    app.run(debug=False)

