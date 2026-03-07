function contour = loadCemv(filename)
    fid = fopen(filename);
   
    if (fid<0)
        disp('File not found.');
        cemv = {};
        return;
    end
    
    contour = cell(0,0);
    contourCounter = 0;
    while 1
        %Get each line
        lineBuffer = fgetl(fid);
        
        %handle EOF of file
        if ~ischar(lineBuffer), break, end
       
        if isempty(lineBuffer)
            continue 
        end
        %handle pure comment line
        if lineBuffer(1) == '#'
            continue;
        end
        
        if strcmp(lineBuffer,'CONTOUR_COUNT=')
            continue;
        end
        
        if strcmp(lineBuffer,'TOTAL_EDGE_COUNT=')
            continue
        end
        
        oneContour = zeros(0,0);
        edgeCount = 0;
        edgeCounter = 0;
        if strcmp(lineBuffer,'[BEGIN CONTOUR]')
            contourCounter = contourCounter + 1;
            while 1
                lineBuffer = fgetl(fid);
                if isempty(lineBuffer)
                    continue 
                end
                
                
                if strcmp(lineBuffer,'[END CONTOUR]')
                    contour{contourCounter} = oneContour;
                    break;
                end

                if strcmp(lineBuffer(1:10),'EDGE_COUNT')
                    edgeCount = str2num(lineBuffer(12:end));
                    oneContour = zeros(8,edgeCount);
                    continue;
                end
                
                if lineBuffer(1) == ' '
                    edgeCounter = edgeCounter + 1;
                    [pX, pY, pDir, pConf, spX, spY, spDir,spConf] = strread(lineBuffer,'[%f, %f] %f %f [%f, %f] %f %f\n');
                    oneContour(:,edgeCounter) = [pX; pY; pDir; pConf; spX; spY; spDir; spConf];
                end
                
            end
        end
        
    end
end