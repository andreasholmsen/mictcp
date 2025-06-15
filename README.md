# BE RESEAU
## TPs BE Reseau - 3 MIC

## Versions
Nous avons implémenté les 4 versions de mictcp, jusqu'au v4.1.

## Compilation
    make clean; make

et pour le lancer

    ./tsock_texte [-p|-s destination] port
    ./tsock_video [[-p|-s] [-t (tcp|mictcp)]

## Choix d'implémentation
- **Fiabilité partielle** : Pour implémenter la fiabilité partielle du v3, nous avons parti du principe d'une fenêtre glissante, un tableau fg de taille 10, contenant les 10 derniers paquets envoyés et si ils ont bien été recu. 

- **Négociation des pertes** : Pour v4, nous avons choisi d'envoyer la proposition de max_perte côté serveur dans le même pdu que le SYNACK, et celui du client par ACK (après avoir fait un choix duquel à choisir). Nous avons décidé que la proposition du client va être choisi si c'est inférieure à celui du serveur, sinon nous prenons la moyenne des deux. Comme c'est le client qui demande un service, nous pensons que c'est juste qu'il peut avoir plus d'impact sur le choix de max_perte et avoir le dernier mot, tout en gardant un possibilité pour le serveur de partager son avis.

- **Gestion des seq_num et ack** : Dans les anciens versions, on a utilisé des variables "next_seq_num" et "seq_attendu" pour gerer les PDU attendus. Mais en v4 on a utilise seqTracker[] qui contient deux int: le premier pour le gestion des seq_num et le deuxième pour le gestion des ack. 

## Notes
 - Il était difficile de mettre en œuvre une solution élégante pour donner au serveur et au client deux suggestions différentes pour max_perte sans modifier client.c et server.c. Nous avons donc utilisé rand() pour illustrer comment on peut choisir deux chiffres différents, et utilisé les paquets SYNACK et ACK pour parvenir à un accord. Nous pensons que la difficulté réside dans les race conditions entre toutes les fonctions, qui font qu'il est difficile de s'arrêter et de laisser l'utilisateur taper un chiffre. Il a donc l'air que V4 est plus stable que V4.1, mais les deux marchent bien
 - dans V4, nous constatons que le serveur ne s'initialise pas correctement avant que le client commence d'envoyer le premier PDU. Le client retransmet immédiatement le paquet perdu, ce qui ne pose donc pas de problème, mais nous pensons qu'il s'agit également d'un problème de race conditions entre les threads.

## Comparaison mictcp V4 et TCP

Dans une environnement avec beaucoup des pertes, on voit clairement que le MICTCP est plus efficace et donne une meilleure experience pour l'utilisateur. Heureusement, le taux de pertes sont bas dans la vrai vie (et pas 20%-50% comme on a testé).

## Reflecion
Il serait intéressant de trouver un moyen d'extraire les images par seconde d'une vidéo et de les utiliser pour déterminer le taux de perte. Les yeux n'ont besoin que de 24 à 30 images par seconde pour percevoir un mouvement fluide, ce qui nous permettrait de prédire plus facilement un bon taux de perte.
