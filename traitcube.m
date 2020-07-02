% this draws the 3D cube scatter of x-traits of the population
% if there are 4 traits, 4th dimension is color
% import traitdata
% select the time you want to plot the data for
% paste the selected portion of traitdata into a new workspace
% rename the workspace as "tdata"

close all
clear all

traits = importdata('simtraits.txt',' ', 1);
tdata=traits.data;

tdata(:,1)=[];

% this is for the 'size' of the dots on the plot
% make sure the size of B matches that of tdata
B=ones(4095,1);

% plots the scatter 
scatter3(tdata(:,1),tdata(:,2),tdata(:,3),B.*30,(tdata(:,4)),'filled');
colormap(winter);
box;
axis([0 1 0 1 0 1 0 1]);
xlabel('x1');
ylabel('x2');
zlabel('x3');
colorbar ('location','East');
caxis([0 1]);
view(30,10);

figure;
m=min(tdata);
subplot(2,2,1)
hist(tdata(:,1),20-floor(m(1,1)*10)*2);
xlim([0 1]);
xlabel('x1');

subplot(2,2,2)
hist(tdata(:,2),20-floor(m(1,2)*10)*2);
xlim([0 1]);
xlabel('x2');

subplot(2,2,3)
hist(tdata(:,3),20-floor(m(1,3)*10)*2);
xlim([0 1]);
xlabel('x3');

subplot(2,2,4)
hist(tdata(:,4),20-floor(m(1,4)*10)*2);
xlim([0 1]);
xlabel('x4');