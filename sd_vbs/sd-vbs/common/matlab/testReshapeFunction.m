function testReshapeFunction

matrixSize = zeros(1, 2);
matrixSize(1) = 4;
matrixSize(2) = 5;
inputMatrix = zeros(matrixSize(1), matrixSize(2));

for i=1:matrixSize(1)
    for j=1:matrixSize(2)
        inputMatrix(i,j) = i+j;
    end
end

outputMatrix = reshapeMatrix(inputMatrix, matrixSize);
actualOut = reshape(inputMatrix, matrixSize);

if outputMatrix == actualOut
    disp('SUCCESS');
else
    disp('ERROR');
end