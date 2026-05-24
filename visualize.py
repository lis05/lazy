#!/bin/env python3
import sys
import os
import glob
import pandas as pd
import numpy as np
import plotly.express as px

def generate_dynamic_dashboard(csv_path):
    if os.path.isdir(csv_path):
        csv_files = glob.glob(os.path.join(csv_path, "*.csv"))
        if not csv_files:
            print(f"Error: No CSV files found in directory '{csv_path}'.")
            sys.exit(1)
        output_filename = os.path.join(csv_path, "combined_dashboard.html")
    else:
        csv_files = [csv_path]
        output_filename = csv_path.replace('.csv', '_dashboard.html')

    df_list = []
    for file in csv_files:
        try:
            temp_df = pd.read_csv(file)
            base_name = os.path.splitext(os.path.basename(file))[0]
            temp_df['file_name'] = base_name
            df_list.append(temp_df)
        except Exception as e:
            print(f"Warning: Failed to read '{file}'. Skipping. Error: {e}")

    if not df_list:
        print("Error: No data could be loaded.")
        sys.exit(1)

    df = pd.concat(df_list, ignore_index=True)

    # Add logarithm of enc_time
    df['log_enc_time'] = np.log10(df['enc_time'].replace(0, np.nan))

    for col in ['-O', '--lazy-matching']:
        if col in df.columns:
            df[col] = df[col].fillna(False).astype(int)

    metrics = ['ratio', 'enc_time', 'log_enc_time', 'dec_time', 'enc_size', 'orig_size']
    ignore_cols = metrics + ['file_name']
    parameters = [col for col in df.columns if col not in ignore_cols]

    initial_x = parameters[0] if parameters else df.columns[0]
    initial_y = 'ratio' if 'ratio' in df.columns else metrics[0]
    initial_color = parameters[1] if len(parameters) > 1 else parameters[0]

    fig = px.scatter(
        df,
        x=initial_x,
        y=initial_y,
        color=initial_color,
        symbol='file_name',
        template="plotly_white",
        color_continuous_scale="viridis"
    )

    hover_lines = [f"{col}=%{{customdata[{i}]}}" for i, col in enumerate(df.columns)]
    hovertemplate = "<br>".join(hover_lines) + "<extra></extra>"

    for trace in fig.data:
        trace_df = df[df['file_name'] == trace.name]
        trace.customdata = trace_df.to_numpy()
        trace.hovertemplate = hovertemplate
        trace.marker.size = 11
        trace.marker.opacity = 0.8
        trace.marker.line.width = 1
        trace.marker.line.color = 'DarkSlateGrey'

    def create_dropdown_buttons(target_axis, target_options):
        buttons = []
        for col in target_options:
            args = {}
            layout_update = {}
            
            trace_values = [df[df['file_name'] == trace.name][col].values for trace in fig.data]

            if target_axis == 'x':
                args['x'] = trace_values
                layout_update['xaxis'] = {'title': {'text': col}}
            elif target_axis == 'y':
                args['y'] = trace_values
                layout_update['yaxis'] = {'title': {'text': col}}
            elif target_axis == 'color':
                args['marker.color'] = trace_values
                if pd.api.types.is_numeric_dtype(df[col]):
                    layout_update['coloraxis'] = {
                        'cmin': float(df[col].min()),
                        'cmax': float(df[col].max()),
                        'colorscale': 'viridis',
                        'colorbar': {
                            'title': {'text': col},
                            'x': 1.02,
                            'yanchor': 'middle',
                            'y': 0.5
                        }
                    }

            buttons.append(dict(
                method="update",
                label=col,
                args=[args, layout_update]
            ))
        return buttons

    fig.update_layout(
        margin=dict(t=180, r=200, l=80, b=80),
        legend=dict(
            title_text="Source File",
            yanchor="top",
            y=0.9,
            xanchor="left",
            x=1.15
        ),
        coloraxis_colorbar=dict(
            title=initial_color,
            x=1.02,
            yanchor="middle",
            y=0.5
        ),
        title={
            'text': "<b>Interactive LZ77 Benchmark Explorer</b>",
            'y': 0.96,
            'x': 0.01,
            'xanchor': 'left',
            'yanchor': 'top',
            'font': {'size': 20}
        },
        updatemenus=[
            dict(
                buttons=create_dropdown_buttons('x', parameters + metrics),
                direction="down", showactive=True,
                x=0.01, xanchor="left", y=1.12, yanchor="top"
            ),
            dict(
                buttons=create_dropdown_buttons('y', metrics + parameters),
                direction="down", showactive=True,
                x=0.35, xanchor="left", y=1.12, yanchor="top"
            ),
            dict(
                buttons=create_dropdown_buttons('color', parameters),
                direction="down", showactive=True,
                x=0.69, xanchor="left", y=1.12, yanchor="top"
            )
        ],
        annotations=[
            dict(text="<b>X Axis</b>", showarrow=False, x=0.01, y=1.18, yref="paper", xanchor="left", yanchor="bottom"),
            dict(text="<b>Y Axis</b>", showarrow=False, x=0.35, y=1.18, yref="paper", xanchor="left", yanchor="bottom"),
            dict(text="<b>Color Scaling</b>", showarrow=False, x=0.69, y=1.18, yref="paper", xanchor="left", yanchor="bottom")
        ]
    )

    fig.write_html(output_filename)
    print(f"Dashboard saved to {output_filename}")
    fig.show()

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python dashboard.py <results_dir_or_file.csv>")
        sys.exit(1)
        
    generate_dynamic_dashboard(sys.argv[1])
