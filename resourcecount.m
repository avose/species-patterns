% this counts the number of resources present in the landscape
% both along rows and columns
% copy and paste *.newrd file into a kwrite
% delete the last line with the generation
% and save it as res.txt

close all
clear all

resources = importdata('res.txt',' ', 1);
res=resources.data;

% delete the extra row/column (fractal creates odd numbered landscapes)
res(:,65)=[];

r=0;
rowsum=zeros(4,64);
c=0;
columnsum=zeros(4,64);

for r=1:64;
  for c=1:64;
    if res(r,c)==0;
       rowsum(1,r)=rowsum(1,r)+1;
       columnsum(1,c)=columnsum(1,c)+1;
    end
    if res(r,c)==1;
       rowsum(2,r)=rowsum(2,r)+1;
       columnsum(2,c)=columnsum(2,c)+1;
    end
    if res(r,c)==2;
       rowsum(3,r)=rowsum(3,r)+1;
       columnsum(3,c)=columnsum(3,c)+1;
    end
    if res(r,c)==3;
       rowsum(4,r)=rowsum(4,r)+1;
       columnsum(4,c)=columnsum(4,c)+1;
    end
  end
end

x=1:64;
p1=polyfit(x,rowsum(1,:),1);
p2=polyfit(x,rowsum(2,:),1);
p3=polyfit(x,rowsum(3,:),1);
p4=polyfit(x,rowsum(4,:),1);
p5=polyfit(x,columnsum(1,:),1);
p6=polyfit(x,columnsum(2,:),1);
p7=polyfit(x,columnsum(3,:),1);
p8=polyfit(x,columnsum(4,:),1);
x1=x*p1(1,1)+p1(1,2);
x2=x*p2(1,1)+p2(1,2);
x3=x*p3(1,1)+p3(1,2);
x4=x*p4(1,1)+p4(1,2);
x5=x*p5(1,1)+p5(1,2);
x6=x*p6(1,1)+p6(1,2);
x7=x*p7(1,1)+p7(1,2);
x8=x*p8(1,1)+p8(1,2);

subplot(1,2,1)
plot(x,rowsum(1,:),'-b',x,x1,':b',x,rowsum(2,:),'-g',x,x2,':g',x,rowsum(3,:),'-r',x,x3,':r',x,rowsum(4,:),'-c',x,x4,':c',x,16,'-k')
ylim([0 50])
xlim([0 64])
title('row sum')
subplot(1,2,2)
plot(x,columnsum(1,:),'-b',x,x5,':b',x,columnsum(2,:),'-g',x,x6,':g',x,columnsum(3,:),'-r',x,x7,':r',x,columnsum(4,:),'-c',x,x8,':c',x,16,'-k')
xlim([0 64])
ylim([0 50])
title('column sum')

    