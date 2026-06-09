import pandas as pd
import plotly.graph_objects as go
from dash import Dash, dcc, html, Input, Output
import os

# Load data
file_path = "stats/hashchains.csv"
if not os.path.exists(file_path):
    raise FileNotFoundError(f"Missing resource: {file_path}")

df = pd.read_csv(file_path)
prefix_options = sorted(df["prefix_length"].unique())

# Initialize Dash application
app = Dash(__name__)

app.layout = html.Div(
    style={"fontFamily": "sans-serif", "padding": "20px"},
    children=[
        html.H2("Hash Chain Length Distribution"),
        html.Div(
            style={"width": "300px", "marginBottom": "20px"},
            children=[
                html.Label("Select Prefix Length:"),
                dcc.Dropdown(
                    id="prefix-dropdown",
                    options=[{"label": str(pl), "value": pl} for pl in prefix_options],
                    value=prefix_options[0] if prefix_options else None,
                    clearable=False,
                ),
            ],
        ),
        dcc.Graph(id="distribution-plot"),
        html.Div(
            id="stats-summary",
            style={
                "marginTop": "20px",
                "padding": "15px",
                "backgroundColor": "#f8f9fa",
                "borderRadius": "5px",
                "fontSize": "16px",
                "lineHeight": "1.6",
            },
        ),
    ],
)


@app.callback(
    [Output("distribution-plot", "figure"), Output("stats-summary", "children")],
    Input("prefix-dropdown", "value"),
)
def update_graph(selected_prefix):
    if selected_prefix is None:
        return go.Figure(), ""

    sub_df = df[df["prefix_length"] == selected_prefix].copy()

    # Sort by hashchain_length to ensure continuous line layout
    sub_df = sub_df.sort_values(by="hashchain_length")

    total_count = sub_df["count"].sum()
    if total_count > 0:
        sub_df["percentage"] = (sub_df["count"] / total_count) * 100
    else:
        sub_df["percentage"] = 0.0

    # Hover text configuration (shows real value explicitly)
    hover_text = [
        f"Chain Length (Exact): {int(l)}<br>Count: {c}<br>Percentage: {p:.2f}%"
        for l, c, p in zip(
            sub_df["hashchain_length"], sub_df["count"], sub_df["percentage"]
        )
    ]

    fig = go.Figure(
        data=[
            go.Scatter(
                x=sub_df["hashchain_length"],
                y=sub_df["count"],
                mode="lines+markers",
                hovertext=hover_text,
                hoverinfo="text",
                line=dict(color="royalblue", width=2),
                marker=dict(size=6),
            )
        ]
    )

    fig.update_layout(
        title=f"Distribution Breakdown for prefix_length {selected_prefix}",
        xaxis=dict(title="Hashchain Length (Log Scale)", type="log", tickformat="d"),
        yaxis=dict(title="Number of Hashchains"),
        template="plotly_white",
    )

    # Calculate statistics for chains based on mm
    summary_children = []
    if not sub_df.empty:
        mm_val = sub_df["mm"].iloc[0]

        # Chains exceeding mm
        exceeding_chains = sub_df[sub_df["hashchain_length"] > mm_val]
        exc_count = exceeding_chains["count"].sum()
        exc_length = (
            exceeding_chains["hashchain_length"] * exceeding_chains["count"]
        ).sum()

        # Chains below mm
        sub_chains = sub_df[sub_df["hashchain_length"] < mm_val]
        sub_count = sub_chains["count"].sum()
        sub_length = (sub_chains["hashchain_length"] * sub_chains["count"]).sum()

        summary_children = [
            html.Strong("Configuration (mm): "),
            f"{mm_val}",
            html.Br(),
            html.Hr(style={"margin": "10px 0", "borderColor": "#ddd"}),
            html.Strong(f"Hash chains with length > {mm_val}: "),
            f"{exc_count:,}",
            html.Br(),
            html.Strong(f"Total length of chains > {mm_val}: "),
            f"{exc_length:,}",
            html.Br(),
            html.Hr(style={"margin": "10px 0", "borderColor": "#ddd"}),
            html.Strong(f"Hash chains with length < {mm_val}: "),
            f"{sub_count:,}",
            html.Br(),
            html.Strong(f"Total length of chains < {mm_val}: "),
            f"{sub_length:,}",
        ]

    return fig, summary_children


if __name__ == "__main__":
    app.run(debug=True)
