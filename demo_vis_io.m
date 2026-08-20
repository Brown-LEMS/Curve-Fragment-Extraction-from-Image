addpath (genpath('util/'));

visualize_contours( ...
    'example_data/cabinet.png', ...
    'example_data/cabinet.edg', ...
    'example_data/cabinet.cem' ...
);


visualize_contours( ...
    'example_data/cabinet.png', ...
    'out.edg', ...
    'out.cem' ...
);

function visualize_contours(image_path, edg_path, cem_path)

    addpath(genpath('util/'));

    % load image
    img = imread(image_path);
    [h, w, ~] = size(img);

    % load edges
    [edges, edgemap, thetamap] = load_edg(edg_path);

    % load contours
    [CEM, edges, cfrags_idx] = load_contours(cem_path);

    % visualize edges
    figure;
    imshow(edgemap, 'border', 'tight');
    pause(0.3);

    % visualize contours
    figure;

    contour_min_length = 2;
    rand_color = 1;

    imshow(img, 'border', 'tight');
    hold on;

    draw_contours(CEM{2}, contour_min_length, rand_color);

    % Optional: output edges and contour files
    % write_cem('cabinet.cem', CEM{2}, h, w);
    % save_edg('cabinet.edg', edges, [w, h]);

end