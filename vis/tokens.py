import os
import numpy as np
import pandas as pd
import plotly.graph_objects as go
from dash import Dash, dcc, html, Input, Output
from scipy.stats import gaussian_kde

file_path = "stats/tokens.csv"
if not os.path.exists(file_path):
    raise FileNotFoundError(f"Missing resource: {file_path}")

total_lits = 0
total_matches = 0

match_entropy_list = []
match_distance_list = []
match_length_list = []

all_lens_list = []
all_entropy_list = []

for chunk in pd.read_csv(
    file_path,
    chunksize=1000000,
    usecols=['token_type', 'i', 'estimated_entropy', 'distance', 'length'],
    dtype={
        'token_type': 'category',
        'i': np.int32,
        'estimated_entropy': np.float32,
        'distance': np.float32,
        'length': np.float32
    }
):
    total_lits += (chunk['token_type'] == 'lit').sum()
    m_mask = chunk['token_type'] == 'match'
    total_matches += m_mask.sum()
    
    lens = chunk['length'].to_numpy()
    lens = np.where(chunk['token_type'] == 'lit', 1, lens)
    lens = np.nan_to_num(lens, nan=1).astype(np.int32)
    all_lens_list.append(lens)
    
    all_entropy_list.append(chunk['estimated_entropy'].to_numpy())
    
    m_chunk = chunk[m_mask]
    if not m_chunk.empty:
        match_entropy_list.append(m_chunk['estimated_entropy'].to_numpy())
        match_distance_list.append(m_chunk['distance'].to_numpy())
        match_length_list.append(m_chunk['length'].to_numpy())

all_lens = np.concatenate(all_lens_list) if all_lens_list else np.array([], dtype=np.int32)
all_entropy = np.concatenate(all_entropy_list) if all_entropy_list else np.array([], dtype=np.float32)

match_entropy = np.concatenate(match_entropy_list) if match_entropy_list else np.array([], dtype=np.float32)
match_distance = np.concatenate(match_distance_list) if match_distance_list else np.array([], dtype=np.float32)
match_length = np.concatenate(match_length_list) if match_length_list else np.array([], dtype=np.float32)

del match_entropy_list, match_distance_list, match_length_list, all_lens_list, all_entropy_list

if len(all_lens) > 0:
    cum_lens = np.cumsum(all_lens)
    total_file_len = cum_lens[-1]
    
    if total_file_len > 0:
        percent_bin = (cum_lens / total_file_len * 100).astype(np.int32)
        np.clip(percent_bin, 0, 99, out=percent_bin)
        percent_bin += 1
        
        bin_counts = np.bincount(percent_bin, minlength=101)[1:101]
        bin_sums = np.bincount(percent_bin, weights=all_entropy, minlength=101)[1:101]
        
        timeline_bins = np.arange(1, 101)
        timeline_entropy = np.zeros(100, dtype=np.float32)
        valid_bins = bin_counts > 0
        timeline_entropy[valid_bins] = bin_sums[valid_bins] / bin_counts[valid_bins]
    else:
        timeline_bins = np.array([])
        timeline_entropy = np.array([])
else:
    timeline_bins = np.array([])
    timeline_entropy = np.array([])

del all_lens, all_entropy

app = Dash(__name__)

app.layout = html.Div(style={'fontFamily': 'sans-serif', 'padding': '20px'}, children=[
    html.H2("Compression Token Analysis Dashboard"),
    
    dcc.Tabs(id="analysis-tabs", value='entropy-dist', children=[
        dcc.Tab(label='1. Matches Entropy Density Curve', value='entropy-dist'),
        dcc.Tab(label='2. Distance Distribution (1% Buckets)', value='distance-dist'),
        dcc.Tab(label='3. Length Distribution (1% Buckets)', value='length-dist'),
        dcc.Tab(label='4. Cumulative Length Contribution', value='length-contrib'),
        dcc.Tab(label='5. Entropy Profile (1% Timeline Chunks)', value='entropy-timeline'),
        dcc.Tab(label='6. Cumulative Distance Contribution', value='distance-contrib'),
    ]),
    
    html.Div(style={'marginTop': '20px'}, children=[
        dcc.Graph(id='tab-graph')
    ]),
    
    html.Div(id='token-summary', style={
        'marginTop': '20px', 
        'padding': '15px', 
        'backgroundColor': '#f8f9fa', 
        'borderRadius': '5px',
        'fontSize': '16px',
        'lineHeight': '1.6'
    }, children=[
        html.Strong("Total Literals (lit): "), f"{total_lits:,}", html.Br(),
        html.Strong("Total Matches (match): "), f"{total_matches:,}"
    ])
])

@app.callback(
    Output('tab-graph', 'figure'),
    Input('analysis-tabs', 'value')
)
def update_graph(selected_tab):
    if total_lits == 0 and total_matches == 0:
        return go.Figure()

    if selected_tab == 'entropy-dist':
        if len(match_entropy) == 0:
            return go.Figure()
            
        min_val = match_entropy.min()
        max_val = match_entropy.max()
        
        if min_val == max_val:
            x_vals = np.linspace(min_val - 1, min_val + 1, 3)
            y_vals = np.array([0.0, 1.0, 0.0])
        else:
            if len(match_entropy) > 100000:
                rng = np.random.default_rng(42)
                kde_sample = rng.choice(match_entropy, size=100000, replace=False)
            else:
                kde_sample = match_entropy
                
            if np.var(kde_sample) == 0:
                x_vals = np.linspace(min_val - 1, min_val + 1, 3)
                y_vals = np.array([0.0, 1.0, 0.0])
            else:
                kde = gaussian_kde(kde_sample)
                x_vals = np.linspace(min_val, max_val, 500)
                y_vals = kde(x_vals)
        
        hover_text = [
            f"Estimated Entropy: {x:.4f}<br>Density: {y:.4f}"
            for x, y in zip(x_vals, y_vals)
        ]

        fig = go.Figure(data=[
            go.Scatter(
                x=x_vals,
                y=y_vals,
                mode='lines',
                hovertext=hover_text,
                hoverinfo='text',
                line=dict(color='teal', width=2.5, shape='spline'),
                fill='tozeroy',
                fillcolor='rgba(0, 128, 128, 0.1)'
            )
        ])
        fig.update_layout(
            title="Match Estimated Entropy Density Curve (Smooth KDE)",
            xaxis=dict(title="Estimated Entropy", range=[min_val, max_val]),
            yaxis=dict(title="Density"),
            template="plotly_white"
        )

    elif selected_tab == 'distance-dist':
        if len(match_distance) == 0:
            return go.Figure()
            
        min_val = match_distance.min()
        max_val = match_distance.max()
        if min_val == max_val:
            max_val += 1
            
        bins = np.linspace(min_val, max_val, 101)
        counts, edges = np.histogram(match_distance, bins=bins)
        bin_centers = (edges[:-1] + edges[1:]) / 2
        
        hover_text = [
            f"Range: [{int(edges[i])} - {int(edges[i+1])})<br>Count: {counts[i]}"
            for i in range(len(counts))
        ]

        fig = go.Figure(data=[
            go.Scatter(
                x=bin_centers,
                y=counts,
                mode='lines+markers',
                hovertext=hover_text,
                hoverinfo='text',
                line=dict(color='coral', width=2),
                marker=dict(size=4)
            )
        ])
        fig.update_layout(
            title="Match Distance Distribution (100 Uniform Buckets)",
            xaxis=dict(title="Distance"),
            yaxis=dict(title="Count"),
            template="plotly_white"
        )

    elif selected_tab == 'length-dist':
        if len(match_length) == 0:
            return go.Figure()
            
        min_val = match_length.min()
        max_val = match_length.max()
        if min_val == max_val:
            max_val += 1
            
        bins = np.linspace(min_val, max_val, 101)
        counts, edges = np.histogram(match_length, bins=bins)
        bin_centers = (edges[:-1] + edges[1:]) / 2
        
        hover_text = [
            f"Range: [{edges[i]:.1f} - {edges[i+1]:.1f})<br>Count: {counts[i]}"
            for i in range(len(counts))
        ]

        fig = go.Figure(data=[
            go.Scatter(
                x=bin_centers,
                y=counts,
                mode='lines+markers',
                hovertext=hover_text,
                hoverinfo='text',
                line=dict(color='purple', width=2),
                marker=dict(size=4)
            )
        ])
        fig.update_layout(
            title="Match Length Distribution (100 Uniform Buckets)",
            xaxis=dict(title="Length"),
            yaxis=dict(title="Count"),
            template="plotly_white"
        )

    elif selected_tab == 'length-contrib':
        if len(match_length) == 0:
            return go.Figure()
            
        min_val = match_length.min()
        max_val = match_length.max()
        if min_val == max_val:
            max_val += 1
            
        bins = np.linspace(min_val, max_val, 101)
        contrib, edges = np.histogram(match_length, bins=bins, weights=match_length)
        cum_contrib = np.cumsum(contrib)
        bin_centers = (edges[:-1] + edges[1:]) / 2
        
        hover_text = [
            f"Length &le; {edges[i+1]:.1f}<br>Cumulative Contribution: {cum_contrib[i]:,.1f} bytes"
            for i in range(len(cum_contrib))
        ]

        fig = go.Figure(data=[
            go.Scatter(
                x=bin_centers,
                y=cum_contrib,
                mode='lines',
                hovertext=hover_text,
                hoverinfo='text',
                line=dict(color='firebrick', width=2.5, shape='spline'),
                fill='tozeroy',
                fillcolor='rgba(178, 34, 34, 0.1)'
            )
        ])
        fig.update_layout(
            title="Cumulative Match Length Contribution (Sum of lengths for all matches &le; L)",
            xaxis=dict(title="Length (L)"),
            yaxis=dict(title="Cumulative Contribution (Bytes)"),
            template="plotly_white"
        )

    elif selected_tab == 'distance-contrib':
        if len(match_distance) == 0 or len(match_length) == 0:
            return go.Figure()
            
        min_val = match_distance.min()
        max_val = match_distance.max()
        if min_val == max_val:
            max_val += 1
            
        bins = np.linspace(min_val, max_val, 101)
        contrib, edges = np.histogram(match_distance, bins=bins, weights=match_length)
        cum_contrib = np.cumsum(contrib)
        bin_centers = (edges[:-1] + edges[1:]) / 2
        
        hover_text = [
            f"Distance &le; {edges[i+1]:.1f}<br>Cumulative Contribution: {cum_contrib[i]:,.1f} bytes"
            for i in range(len(cum_contrib))
        ]

        fig = go.Figure(data=[
            go.Scatter(
                x=bin_centers,
                y=cum_contrib,
                mode='lines',
                hovertext=hover_text,
                hoverinfo='text',
                line=dict(color='darkorange', width=2.5, shape='spline'),
                fill='tozeroy',
                fillcolor='rgba(255, 140, 0, 0.1)'
            )
        ])
        fig.update_layout(
            title="Cumulative Match Distance Contribution (Sum of lengths for all matches with distance &le; D)",
            xaxis=dict(title="Distance (D)"),
            yaxis=dict(title="Cumulative Contribution (Bytes)"),
            template="plotly_white"
        )

    elif selected_tab == 'entropy-timeline':
        if len(timeline_bins) == 0:
            return go.Figure()
        
        hover_text = [
            f"File Covered: {int(b)}%<br>Mean Entropy: {e:.4f}"
            for b, e in zip(timeline_bins, timeline_entropy)
        ]

        fig = go.Figure(data=[
            go.Scatter(
                x=timeline_bins,
                y=timeline_entropy,
                mode='lines+markers',
                hovertext=hover_text,
                hoverinfo='text',
                line=dict(color='royalblue', width=2),
                marker=dict(size=4)
            )
        ])
        fig.update_layout(
            title="Mean Estimated Entropy Profile over File Processing Timeline (1% Covered Bytes Chunks)",
            xaxis=dict(title="File Progress (% Bytes Covered)", tickmode='linear', tick0=0, dtick=10),
            yaxis=dict(title="Mean Estimated Entropy"),
            template="plotly_white"
        )

    return fig

if __name__ == '__main__':
    app.run(debug=True)

