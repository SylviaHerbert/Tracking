classdef ObstacleMap < handle
    properties
        rrt; % the stored RRT
        global_obs; % a cell array of all the obstacles on the map.
        local_obs = cell(4, 3); % a cell array of the obstacles that the drone has seen so far.
        cube; % the cube sensing range
        track_err; % the tracking error bound to pad each obstacle with
    end
    
    methods
        % initialization
        % if input point, might not need entire rrt? Find another way of
        % getting global_obs
        function self = ObstacleMap(rrt, point, senseRange, trackErrBnd)
            self.rrt = rrt;
            if nargin == 4
                self.global_obs = rrt.obs;
                self.cube = MakeCubeRange(point, senseRange);
                self.track_err = trackErrBnd;                
            end
        end
        
        % sense for obstacles at the *point* input then update local/known 
        % obstacles. use the sensor's sensing *range*. overwrite the original matrix 
        % with the new matrix (then this must be modified within the 
        % Run method of RRTPlanner)
        
        function SenseAndUpdate(point, range)
            % sense
            senseRange = MakeCubeRange(point, range);
            % all the obstacles that have not been explored yet
            other_obs = setdiff(self.global_obs, self.local_obs);
            if ~isempty(other_obs) % if there are still some other unexplored obstacles...
                % check for obstacle intersection with each face that makes up
                % the sensing range
                for face = senseRange
                    for obst = other_obs
                        obs = CheckIntersection(obst, face); % just use global_obs and index the same way as 
                                                      % local_obs so update part into whole obs is easier...
                        % update local_obs here
                    end
                end
            end
        end
        
        % helper function for SenseAndUpdate. Creates the sensing radius
        % and cube data structure used for sensing
        % point: center of the cube
        % size: length of a side of the cube
        function cube = MakeCubeRange(point, size)
            cube = [];
            cube(:,1) = [point(1) + size/2, point(2) + size/2, point(3) + size/2];
            cube(:,2) = [point(1) + size/2, point(2) - size/2, point(3) + size/2];
            cube(:,3) = [point(1) + size/2, point(2) + size/2, point(3) - size/2];
            cube(:,4) = [point(1) + size/2, point(2) - size/2, point(3) - size/2];
            cube(:,5) = [point(1) - size/2, point(2) + size/2, point(3) + size/2];
            cube(:,6) = [point(1) - size/2, point(2) - size/2, point(3) + size/2];
            cube(:,7) = [point(1) - size/2, point(2) + size/2, point(3) - size/2];
            cube(:,8) = [point(1) - size/2, point(2) - size/2, point(3) - size/2];
        end
        
        % helper function for SenseAndUpdate. Checks if there's any
        % intersection between an obstacle and a plane (a sensor cube's face)
        % and return the two separated parts of the obstacle in an array.
        
        % Plane is a set of coordinates such that when viewed from inside the cube, the points 
        % are in a counterclockwise orientation?
        % Then normal vector N points inwards and check sign of dot product with
        % point P on cube face and Q on obs 1 or 2. v = dot(Q - P, N);
        
        % plane is a 4x3 array
        function CheckIntersection(obst, cube)
            % compute the coordinates' ranges of the cube
            mostPositive = cube(:,1);
            mostNegative = cube(:,8);
            oneCubeRan = [mostPositive(1), mostNegative(1)];
            twoCubeRan = [mostPositive(2), mostNegative(2)];
            threeCubeRan = [mostPositive(3), mostNegative(3)];
            
            for obs = obst % iterate over the global obst set
                numInRange = 0; % count the number of corners of the plane that are within the cube
                for point = obs % iterate over the four points that make up the obs
                    if point(1)
                        numInRange = numInRange + 1;
                    end
                    
                    if numInRange == 3
                        % update local obs to combine entire obs
                        % pad the obs with track err
                        % break the loop early
                    end
                end
                if numInRange == 2
                    % cut off the local obs at sense range. Do the brute
                    % force checking
                end
            end
        end
        
    end
end