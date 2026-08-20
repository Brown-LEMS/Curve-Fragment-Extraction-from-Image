addpath (genpath('util/'));

img = imread('example_data/cabinet.png');
[h,w,~]= size(img);

% load in edges and contours
% [edges, edgemap, thetamap] = load_edg('example_data/cabinet.edg');
% [CEM, edges, cfrags_idx] = load_contours('example_data/cabinet.cem');

[edges, edgemap, thetamap] = load_edg('out.edg');
[CEM, edges, cfrags_idx] = load_contours('out.cem');


% visualize edges
figure(1);
imshow(edgemap, 'border', 'tight');

pause(0.3);

% visualize contours
figure(2);
contour_min_length = 2;
rand_color = 1;
imshow(img, 'border', 'tight'); 
hold on;
draw_contours(CEM{2}, contour_min_length, rand_color);

%> Optional: output edges and contour files
% write_cem('cabinet.cem', CEM{2}, h, w)
% save_edg('cabinet.edg', edges, [w,h])