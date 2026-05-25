#!/bin/env python3

import sys
import os
import glob
import pandas as pd
import numpy as np
import plotly.express as px

try:
    from dash import Dash, dcc, html, Input, Output
except ImportError:
    print("Error: install dash with: pip install dash pandas plotly")
    sys.exit(1)


def load_data(path):
    if os.path.isdir(path):
        files = glob.glob(os.path.join(path, "*.csv"))
        if not files:
            print("No CSV files found")
            sys.exit(1)
    else:
        files = [path]

    dfs = []
    for f in files:
        try:
            df = pd.read_csv(f)
            df["source_file"] = os.path.basename(f)
            dfs.append(df)
        except Exception as e:
            print(f"skip {f}: {e}")

    if not dfs:
        print("No data loaded")
        sys.exit(1)

    df = pd.concat(dfs, ignore_index=True)

    # ensure numeric types
    for col in ["enc_time", "dec_time", "enc_size", "ratio_percent"]:
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce")

    df["log_enc_time"] = np.log10(df["enc_time"].replace(0, np.nan))
    df["log_dec_time"] = np.log10(df["dec_time"].replace(0, np.nan))

    return df


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python dashboard.py <csv_or_dir>")
        sys.exit(1)

    df = load_data(sys.argv[1])

    metrics = [
        "ratio_percent",
        "enc_time",
        "dec_time",
        "log_enc_time",
        "log_dec_time",
        "enc_size",
    ]

    categorical = ["compressor", "filename", "level", "source_file"]

    columns = [c for c in df.columns if c in metrics + categorical]

    default_x = "log_enc_time" if "log_enc_time" in df.columns else columns[0]
    default_y = "ratio_percent" if "ratio_percent" in df.columns else columns[1]
    default_color = "compressor" if "compressor" in df.columns else columns[0]

    app = Dash(__name__)

    app.layout = html.Div(
        [
            html.H2("Compression Benchmark Explorer"),
            html.Div(
                [
                    html.Label("X Axis"),
                    dcc.Dropdown(columns, default_x, id="x"),
                ],
                style={"width": "250px", "display": "inline-block"},
            ),
            html.Div(
                [
                    html.Label("Y Axis"),
                    dcc.Dropdown(columns, default_y, id="y"),
                ],
                style={"width": "250px", "display": "inline-block"},
            ),
            html.Div(
                [
                    html.Label("Color"),
                    dcc.Dropdown(categorical, default_color, id="color"),
                ],
                style={"width": "250px", "display": "inline-block"},
            ),
            html.Div(
                [
                    html.Label("Compressor Filter"),
                    dcc.Dropdown(
                        options=[
                            {"label": c, "value": c} for c in df["compressor"].unique()
                        ],
                        value=list(df["compressor"].unique()),
                        multi=True,
                        id="compressor_filter",
                    ),
                ]
            ),
            html.Div(
                [
                    html.Label("File Filter"),
                    dcc.Dropdown(
                        options=[
                            {"label": f, "value": f} for f in df["filename"].unique()
                        ],
                        value=list(df["filename"].unique()),
                        multi=True,
                        id="file_filter",
                    ),
                ]
            ),
            dcc.Graph(id="plot", style={"height": "75vh"}),
        ]
    )

    @app.callback(
        Output("plot", "figure"),
        Input("x", "value"),
        Input("y", "value"),
        Input("color", "value"),
        Input("compressor_filter", "value"),
        Input("file_filter", "value"),
    )
    def update(x, y, color, compressors, files):
        import plotly.graph_objects as go

        dff = df[df["compressor"].isin(compressors) & df["filename"].isin(files)]

        if dff.empty:
            return go.Figure()

        fig = go.Figure()

        # group by compressor (optionally include filename if needed)
        for comp, g in dff.groupby("compressor"):
            g = g.copy()

            # sort for meaningful line connection
            if "level" in g.columns:
                g = g.sort_values("level")
            else:
                g = g.sort_values(x)

            fig.add_trace(
                go.Scatter(
                    x=g[x],
                    y=g[y],
                    mode="lines+markers",
                    name=str(comp),
                    line=dict(width=1),  # thin lines
                    marker=dict(size=8, opacity=0.8),
                    customdata=g.to_numpy(),
                    hovertemplate="<br>".join(
                        [
                            f"{col}: %{{customdata[{i}]}}"
                            for i, col in enumerate(g.columns)
                        ]
                    )
                    + "<extra></extra>",
                )
            )

        fig.update_layout(
            template="plotly_white",
            xaxis_title=x,
            yaxis_title=y,
        )

        return fig

    app.run(debug=True)
